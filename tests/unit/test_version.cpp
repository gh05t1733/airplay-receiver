#include <catch2/catch_test_macros.hpp>

#include "common/version.h"

TEST_CASE("build identity is populated", "[common]") {
    REQUIRE(arak::common::productName() == "airplay-receiver");
    REQUIRE_FALSE(arak::common::versionString().empty());
    REQUIRE(arak::common::buildBanner().find("airplay-receiver") != std::string::npos);
}
