// src/common/identity.h — ReceiverIdentity: the single source of truth for advertised values.
// Brief §5.2, PRD §8.1/§8.2.
// Generates and persists an Ed25519 keypair + pairing UUIDs at first run.
#pragma once

#include <string>
#include <cstdint>

namespace arak::identity {

// Transient pairing feature bits (decision D5).
// Bit  7: SupportsAirPlayScreen
// Bit  9: SupportsAirPlayAudio
// Bit 30: RAOP
// Bit 42: SupportsScreenMultiCodec
// Bit 48: SupportsTransientPairing
// Bit 27 (legacy/PIN) is deliberately ABSENT.
constexpr uint64_t kFeaturesTransient = (1ULL << 7) | (1ULL << 9) | (1ULL << 30)
                                       | (1ULL << 42) | (1ULL << 48);

// Status flags. Bit 2 = AirPlay on.
constexpr uint64_t kStatusFlags = 0x4;

// Encode a 64-bit features mask as "0xLOWER,0xUPPER" (the wire format).
std::string encodeFeatures(uint64_t features);

// ReceiverIdentity holds everything needed for TXT/SRV registration.
struct ReceiverIdentity {
    std::string deviceId;              // "aa:bb:cc:dd:ee:ff" from adapter MAC
    std::string name;                  // display name, e.g. "Arakatian PC"
    std::string model = "AppleTV3,2";  // device model string
    std::string sourceVersion = "220.68"; // AirPlay version string
    uint64_t    features = kFeaturesTransient;
    uint64_t    flags = kStatusFlags;
    uint16_t    rtspPort = 7000;       // actual bound port (SRV target)
    // Identity (persisted, generated at first run)
    std::string publicKeyHex;          // 64 hex chars (Ed25519 public key)
    std::string pairingId;             // stable UUID
    std::string systemPairingId;       // stable UUID
};

// Initialize or load the persistent identity from %LOCALAPPDATA%.
// Generates a new Ed25519 keypair on first run.
bool initIdentity(const std::string& dataDir, ReceiverIdentity& out);

// Save the identity to disk.
bool saveIdentity(const std::string& dataDir, const ReceiverIdentity& identity);

}  // namespace arak::identity
