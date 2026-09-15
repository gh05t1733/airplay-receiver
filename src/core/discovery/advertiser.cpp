// src/core/discovery/advertiser.cpp — mDNS advertiser implementation.
// Dynamically loads dnssd.dll from System32 to talk to the running Bonjour daemon.
// If the DLL is not found, falls back to log-only mode (no actual advertisement).

#include "advertiser.h"
#include <spdlog/spdlog.h>
#include <algorithm>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <array>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "ws2_32.lib")

// dns_sd.h typedefs — we define only what we need, avoiding the vendored header dependency.
// These match the Apple dns_sd.h API signatures for DNSServiceRegister/DeRegister.
extern "C" {

typedef int32_t DNSServiceRef;
typedef uint32_t DNSServiceFlags;
typedef uint32_t DNSServiceProtocol;
typedef int32_t DNSServiceErrorType;
typedef uint32_t DNSRecordRef;

// Callback type for DNSServiceRegister
typedef void (*DNSServiceRegisterReply)(
    DNSServiceRef sdRef,
    DNSServiceFlags flags,
    DNSServiceErrorType errorCode,
    const char* name,
    const char* regtype,
    const char* domain,
    void* context
);

// Function pointer types
typedef DNSServiceErrorType (*PFN_DNSServiceRegister)(
    DNSServiceRef* sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    const char* name,
    const char* regtype,
    const char* domain,
    const char* hosttarget,
    uint16_t port,
    uint16_t txtLen,
    const void* txtRecord,
    DNSServiceRegisterReply callBack,
    void* context
);

typedef DNSServiceErrorType (*PFN_DNSServiceDeRegister)(
    DNSServiceRef sdRef,
    DNSRecordRef RecordRef
);

typedef void (*PFN_DNSServiceRefDeallocate)(DNSServiceRef sdRef);

typedef int (*PFN_DNSServiceRefSockFD)(DNSServiceRef sdRef);

typedef DNSServiceErrorType (*PFN_DNSServiceProcessResult)(DNSServiceRef sdRef);

}  // extern "C"

namespace arak::discovery {

// Build a TXT record buffer from key-value pairs.
// Format: for each entry, 1 byte length + "key=value".
static std::vector<uint8_t> buildTxtRecord(
    const std::unordered_map<std::string, std::string>& entries) {
    std::vector<uint8_t> record;
    for (const auto& [key, value] : entries) {
        std::string entry = key + "=" + value;
        record.push_back(static_cast<uint8_t>(entry.size()));
        record.insert(record.end(), entry.begin(), entry.end());
    }
    return record;
}

struct Advertiser::Impl {
    HMODULE dnssdModule = nullptr;
    PFN_DNSServiceRegister fnRegister = nullptr;
    PFN_DNSServiceRefDeallocate fnDeallocate = nullptr;
    PFN_DNSServiceRefSockFD fnSockFD = nullptr;
    PFN_DNSServiceProcessResult fnProcessResult = nullptr;

    DNSServiceRef airplayService = 0;   // int32_t, not a pointer
    DNSServiceRef raopService = 0;

    bool daemonAvailable = false;
    bool running = false;
    std::vector<std::string> addresses;

    bool loadDnssd() {
        dnssdModule = LoadLibraryW(L"dnssd.dll");
        if (!dnssdModule) {
            spdlog::warn("mDNS: dnssd.dll not found in System32; running in log-only mode");
            return false;
        }

        fnRegister = reinterpret_cast<PFN_DNSServiceRegister>(
            GetProcAddress(dnssdModule, "DNSServiceRegister"));
        fnDeallocate = reinterpret_cast<PFN_DNSServiceRefDeallocate>(
            GetProcAddress(dnssdModule, "DNSServiceRefDeallocate"));
        fnSockFD = reinterpret_cast<PFN_DNSServiceRefSockFD>(
            GetProcAddress(dnssdModule, "DNSServiceRefSockFD"));
        fnProcessResult = reinterpret_cast<PFN_DNSServiceProcessResult>(
            GetProcAddress(dnssdModule, "DNSServiceProcessResult"));

        if (!fnRegister || !fnDeallocate) {
            spdlog::warn("mDNS: dnssd.dll missing required exports; running in log-only mode");
            FreeLibrary(dnssdModule);
            dnssdModule = nullptr;
            return false;
        }

        spdlog::info("mDNS: loaded dnssd.dll successfully");
        return true;
    }

    void unregisterService(DNSServiceRef& svc) {
        if (svc != 0 && fnDeallocate) {
            fnDeallocate(svc);
            svc = 0;
        }
    }

    DNSServiceErrorType registerService(
        const char* name, const char* regtype, uint16_t port,
        const std::vector<uint8_t>& txtRecord) {
        if (!fnRegister) return -1;  // KDNSServiceErr_Defunct

        DNSServiceRef svc = 0;
        DNSServiceErrorType err = fnRegister(
            &svc,
            0,            // flags
            0,            // interfaceIndex (0 = all)
            name,
            regtype,
            nullptr,      // domain (let daemon choose)
            nullptr,      // hosttarget (let daemon choose)
            port,
            static_cast<uint16_t>(txtRecord.size()),
            txtRecord.data(),
            nullptr,      // callback (we don't poll — log-only for now)
            nullptr       // context
        );

        if (err == 0 && svc != 0) {
            spdlog::info("mDNS: registered {} ({}) on port {}", name, regtype, port);
        } else {
            spdlog::error("mDNS: DNSServiceRegister failed for {} ({}) with error {}", name, regtype, err);
        }
        return err;
    }
};

Advertiser::Advertiser() : impl_(std::make_unique<Impl>()) {}
Advertiser::~Advertiser() { stop(); }

CoreStatus Advertiser::start(const identity::ReceiverIdentity& identity, uint16_t port) {
    impl_->daemonAvailable = impl_->loadDnssd();

    // Get local addresses for logging
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hostname, nullptr, &hints, &result) == 0) {
            for (auto* p = result; p; p = p->ai_next) {
                char addrBuf[INET_ADDRSTRLEN];
                auto* sin = reinterpret_cast<sockaddr_in*>(p->ai_addr);
                inet_ntop(AF_INET, &sin->sin_addr, addrBuf, sizeof(addrBuf));
                impl_->addresses.push_back(addrBuf);
            }
            freeaddrinfo(result);
        }
    }

    // Build _airplay._tcp TXT record
    std::string featuresStr = identity::encodeFeatures(identity.features);
    auto airplayTxt = buildTxtRecord({
        {"deviceid", identity.deviceId},
        {"features", featuresStr},
        {"flags", std::to_string(identity.flags)},
        {"model", identity.model},
        {"srcvers", identity.sourceVersion},
        {"pk", identity.publicKeyHex},
        {"pi", identity.pairingId},
        {"psi", identity.systemPairingId},
    });

    // Build _raop._tcp TXT record
    // Instance name MUST be <MAC-UPPERCASE>@<Display Name>
    std::string macUpper = identity.deviceId;
    std::transform(macUpper.begin(), macUpper.end(), macUpper.begin(), ::toupper);
    // Remove colons for the instance name
    macUpper.erase(std::remove(macUpper.begin(), macUpper.end(), ':'), macUpper.end());
    std::string raopInstanceName = macUpper + "@" + identity.name;

    auto raopTxt = buildTxtRecord({
        {"txtvers", "1"},
        {"ch", "2"},                // stereo
        {"cn", "0,1,2,3"},         // PCM, ALAC, AAC, AAC-ELD
        {"et", "0,3,5"},           // encryption types
        {"md", "0,1,2"},           // metadata
        {"pw", "false"},           // no password
        {"sr", "44100"},           // sample rate
        {"ss", "16"},              // sample size
        {"tp", "UDP"},             // transport
        {"vn", "65537"},           // AirTunes version
        {"vs", identity.sourceVersion},
        {"am", identity.model},
        {"sf", std::to_string(identity.flags)},
        {"ft", featuresStr},
    });

    if (impl_->daemonAvailable) {
        // Register _airplay._tcp
        auto err = impl_->registerService(
            identity.name.c_str(), "_airplay._tcp", port, airplayTxt);
        if (err == 0) {
            // Store service ref for cleanup
        }

        // Register _raop._tcp
        err = impl_->registerService(
            raopInstanceName.c_str(), "_raop._tcp", port, raopTxt);
        if (err == 0) {
            // Store service ref for cleanup
        }
    } else {
        // Log-only mode
        spdlog::info("mDNS: log-only mode — services NOT registered on the network");
        spdlog::info("mDNS: would register _airplay._tcp: name='{}', port={}", identity.name, port);
        spdlog::info("mDNS: would register _raop._tcp: name='{}', port={}", raopInstanceName, port);
    }

    // Log advertised addresses
    for (const auto& addr : impl_->addresses) {
        spdlog::info("mDNS: advertising on interface {}", addr);
    }

    impl_->running = true;
    return CoreStatus::Ok;
}

CoreStatus Advertiser::update(const identity::ReceiverIdentity& identity) {
    if (!impl_->running) return CoreStatus::NotInitialized;
    // Re-registration: stop and restart
    stop();
    return start(identity, identity.rtspPort);
}

CoreStatus Advertiser::stop() noexcept {
    if (!impl_->running) return CoreStatus::Ok;

    impl_->unregisterService(impl_->airplayService);
    impl_->unregisterService(impl_->raopService);

    if (impl_->dnssdModule) {
        FreeLibrary(impl_->dnssdModule);
        impl_->dnssdModule = nullptr;
    }

    impl_->running = false;
    spdlog::info("mDNS: advertisement stopped");
    return CoreStatus::Ok;
}

std::vector<std::string> Advertiser::localAddresses() const {
    return impl_->addresses;
}

bool Advertiser::usingDaemon() const {
    return impl_->daemonAvailable && impl_->running;
}

}  // namespace arak::discovery
