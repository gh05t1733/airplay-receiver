// tests/unit/test_identity.cpp — Receiver identity persistence tests.
// Gate 1.12: "Ed25519 identity is persisted and identical across two runs."

#include <catch2/catch_test_macros.hpp>
#include "common/identity.h"
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

TEST_CASE("identity: initIdentity generates and persists", "[identity]") {
    // Use a temporary directory for testing
    auto tempDir = fs::temp_directory_path() / "arak_test_identity";
    fs::create_directories(tempDir);

    // Clean up any previous test data
    fs::remove_all(tempDir / "identity");

    arak::identity::ReceiverIdentity id1;
    REQUIRE(arak::identity::initIdentity((tempDir / "identity").string(), id1));
    REQUIRE_FALSE(id1.publicKeyHex.empty());
    REQUIRE_FALSE(id1.pairingId.empty());
    REQUIRE_FALSE(id1.systemPairingId.empty());
    REQUIRE(id1.publicKeyHex.size() == 64);  // 32 bytes = 64 hex chars

    // Load again — should get the same identity
    arak::identity::ReceiverIdentity id2;
    REQUIRE(arak::identity::initIdentity((tempDir / "identity").string(), id2));
    REQUIRE(id1.publicKeyHex == id2.publicKeyHex);
    REQUIRE(id1.pairingId == id2.pairingId);
    REQUIRE(id1.systemPairingId == id2.systemPairingId);

    // Clean up
    fs::remove_all(tempDir);
}

TEST_CASE("identity: encodeFeatures matches known values", "[identity]") {
    // The transient profile features string must match the wire spec
    std::string encoded = arak::identity::encodeFeatures(arak::identity::kFeaturesTransient);
    REQUIRE(encoded == "0x40000280,0x00010400");

    // Lower bits
    uint32_t lower = static_cast<uint32_t>(arak::identity::kFeaturesTransient & 0xFFFFFFFF);
    REQUIRE(lower == 0x40000280);

    // Upper bits
    uint32_t upper = static_cast<uint32_t>((arak::identity::kFeaturesTransient >> 32) & 0xFFFFFFFF);
    REQUIRE(upper == 0x10400);
}

TEST_CASE("identity: ReceiverIdentity default values", "[identity]") {
    arak::identity::ReceiverIdentity id;
    REQUIRE(id.model == "AppleTV3,2");
    REQUIRE(id.sourceVersion == "220.68");
    REQUIRE(id.features == arak::identity::kFeaturesTransient);
    REQUIRE(id.flags == arak::identity::kStatusFlags);
    REQUIRE(id.rtspPort == 7000);
}
