// src/core/discovery/advertiser.h — mDNS/DNS-SD advertisement via the Bonjour daemon.
// Architecture brief C1, PRD §8.1. Registers _airplay._tcp + _raop._tcp through the
// running Bonjour daemon's client API (dnssd.dll). Never binds UDP 5353.
// Falls back to stub logging when dnssd.dll is not available.
#pragma once

#include "common/types.h"
#include "common/identity.h"
#include <string>
#include <vector>

namespace arak::discovery {

class Advertiser {
public:
    Advertiser();
    ~Advertiser();

    // Start advertising both _airplay._tcp and _raop._tcp services.
    // Tries to load dnssd.dll from System32; falls back to log-only mode.
    CoreStatus start(const identity::ReceiverIdentity& identity, uint16_t port);

    // Re-advertise with updated identity (e.g. name change).
    CoreStatus update(const identity::ReceiverIdentity& identity);

    // Stop advertising and deregister services.
    CoreStatus stop() noexcept;

    // Get local addresses that were advertised.
    std::vector<std::string> localAddresses() const;

    // Is the advertiser actually using the Bonjour daemon (vs log-only mode)?
    bool usingDaemon() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace arak::discovery
