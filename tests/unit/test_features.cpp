// tests/unit/test_features.cpp — Features mask encoding and gate 1.11 verification.
// Gate 1.11: "The advertised pairing bits match the implemented profile"
// features = 0x40000280,0x10400 (bits 7/9/30/42/48)

#include <catch2/catch_test_macros.hpp>
#include "common/identity.h"

TEST_CASE("features mask: transient pairing bits are correct", "[features]") {
    using arak::identity::kFeaturesTransient;

    // Bit 7: SupportsAirPlayScreen
    REQUIRE((kFeaturesTransient & (1ULL << 7)) != 0);
    // Bit 9: SupportsAirPlayAudio
    REQUIRE((kFeaturesTransient & (1ULL << 9)) != 0);
    // Bit 30: RAOP
    REQUIRE((kFeaturesTransient & (1ULL << 30)) != 0);
    // Bit 42: SupportsScreenMultiCodec
    REQUIRE((kFeaturesTransient & (1ULL << 42)) != 0);
    // Bit 48: SupportsTransientPairing
    REQUIRE((kFeaturesTransient & (1ULL << 48)) != 0);
}

TEST_CASE("features mask: legacy bit 27 is ABSENT", "[features]") {
    using arak::identity::kFeaturesTransient;

    // Bit 27: SupportsLegacyPairing — deliberately NOT advertised
    REQUIRE((kFeaturesTransient & (1ULL << 27)) == 0);
}

TEST_CASE("features mask: encoded string matches expected", "[features]") {
    using arak::identity::kFeaturesTransient;

    std::string encoded = arak::identity::encodeFeatures(kFeaturesTransient);

    // Expected: 0x40000280,0x00010400
    // Lower 32 bits: bit 7 (0x80) + bit 9 (0x200) + bit 30 (0x40000000) = 0x40000280
    // Upper 32 bits: bit 42-32=10 (0x400) + bit 48-32=16 (0x10000) = 0x00010400
    REQUIRE(encoded == "0x40000280,0x00010400");
}

TEST_CASE("features mask: status flags", "[features]") {
    using arak::identity::kStatusFlags;
    REQUIRE(kStatusFlags == 0x4);
}

TEST_CASE("features mask: encode individual value", "[features]") {
    // Test with a known value
    std::string encoded = arak::identity::encodeFeatures(0x1234567890ABCDEFULL);
    REQUIRE(encoded == "0x90ABCDEF,0x12345678");
}

TEST_CASE("features mask: zero produces correct output", "[features]") {
    std::string encoded = arak::identity::encodeFeatures(0);
    REQUIRE(encoded == "0x00000000,0x00000000");
}
