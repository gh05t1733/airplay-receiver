// src/common/identity.cpp — Receiver identity generation and persistence.
// Uses OpenSSL for Ed25519 keypair generation (architecture brief §3: OpenSSL 3.x, Apache-2.0).

#include "common/identity.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>

// Try OpenSSL for Ed25519; fall back to a UUID-based identity if OpenSSL is unavailable.
#if __has_include(<openssl/evp.h>)
#define ARAK_HAS_OPENSSL 1
#include <openssl/evp.h>
#include <openssl/rand.h>
#else
#define ARAK_HAS_OPENSSL 0
#endif

namespace {

// Generate a UUID v4 string (random).
std::string generateUUID() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    uint32_t a = dist(rng);
    uint16_t b = static_cast<uint16_t>(dist(rng) & 0xFFFF);
    uint16_t c = static_cast<uint16_t>((dist(rng) & 0x0FFF) | 0x4000);  // version 4
    uint16_t d = static_cast<uint16_t>((dist(rng) & 0x3FFF) | 0x8000);  // variant 1

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << a << "-"
        << std::setw(4) << b << "-"
        << std::setw(4) << c << "-"
        << std::setw(4) << d << "-"
        << std::setw(8) << static_cast<uint32_t>(dist(rng));
    return oss.str();
}

// Convert binary data to lowercase hex.
std::string toHex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

// Identity file format: simple key=value pairs (one per line).
// Format:
//   publicKeyHex=<hex>
//   pairingId=<uuid>
//   systemPairingId=<uuid>
bool loadIdentityFile(const std::string& path, arak::identity::ReceiverIdentity& id) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "publicKeyHex") id.publicKeyHex = val;
        else if (key == "pairingId") id.pairingId = val;
        else if (key == "systemPairingId") id.systemPairingId = val;
    }
    return !id.publicKeyHex.empty() && !id.pairingId.empty() && !id.systemPairingId.empty();
}

bool saveIdentityFile(const std::string& path, const arak::identity::ReceiverIdentity& id) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << "publicKeyHex=" << id.publicKeyHex << "\n";
    file << "pairingId=" << id.pairingId << "\n";
    file << "systemPairingId=" << id.systemPairingId << "\n";
    return file.good();
}

#if ARAK_HAS_OPENSSL
bool generateEd25519(arak::identity::ReceiverIdentity& id) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (!ctx) {
        spdlog::error("identity: EVP_PKEY_CTX_new_id(ED25519) failed");
        return false;
    }
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        spdlog::error("identity: EVP_PKEY_keygen failed");
        EVP_PKEY_CTX_free(ctx);
        return false;
    }
    EVP_PKEY_CTX_free(ctx);
    // Extract raw 32-byte public key
    size_t pubLen = 32;
    std::vector<uint8_t> pubKey(pubLen);
    if (EVP_PKEY_get_raw_public_key(pkey, pubKey.data(), &pubLen) != 1) {
        spdlog::error("identity: EVP_PKEY_get_raw_public_key failed");
        EVP_PKEY_free(pkey);
        return false;
    }
    id.publicKeyHex = toHex(pubKey.data(), pubLen);

    // Save private key to a separate file (not advertised)
    // For now we just save the public key — private key persistence is Sprint 2.
    EVP_PKEY_free(pkey);
    return true;
}
#else
bool generateEd25519(arak::identity::ReceiverIdentity& id) {
    // Fallback: generate a random 32-byte "public key" placeholder.
    // This is NOT cryptographically secure — it's a Sprint 1 placeholder.
    // Sprint 2 will add real Ed25519 via OpenSSL.
    std::vector<uint8_t> fakePub(32);
    std::random_device rd;
    std::mt19937_64 rng(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    for (auto& b : fakePub) b = static_cast<uint8_t>(dist(rng));
    id.publicKeyHex = toHex(fakePub.data(), fakePub.size());
    spdlog::warn("identity: OpenSSL not available; using placeholder Ed25519 key");
    return true;
}
#endif

}  // anonymous namespace

namespace arak::identity {

std::string encodeFeatures(uint64_t features) {
    uint32_t lower = static_cast<uint32_t>(features & 0xFFFFFFFF);
    uint32_t upper = static_cast<uint32_t>((features >> 32) & 0xFFFFFFFF);
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << lower
        << ",0x" << std::setw(8) << std::setfill('0') << upper;
    return oss.str();
}

bool initIdentity(const std::string& dataDir, ReceiverIdentity& out) {
    std::filesystem::create_directories(dataDir);
    auto identityFile = std::filesystem::path(dataDir) / "identity.txt";

    if (loadIdentityFile(identityFile.string(), out)) {
        spdlog::info("identity: loaded from {}", identityFile.string());
        return true;
    }

    // First run: generate identity
    spdlog::info("identity: generating new identity (first run)");
    if (!generateEd25519(out)) return false;
    out.pairingId = generateUUID();
    out.systemPairingId = generateUUID();

    if (!saveIdentityFile(identityFile.string(), out)) {
        spdlog::error("identity: failed to save to {}", identityFile.string());
        return false;
    }
    spdlog::info("identity: saved to {}", identityFile.string());
    return true;
}

bool saveIdentity(const std::string& dataDir, const ReceiverIdentity& identity) {
    auto identityFile = std::filesystem::path(dataDir) / "identity.txt";
    return saveIdentityFile(identityFile.string(), identity);
}

}  // namespace arak::identity
