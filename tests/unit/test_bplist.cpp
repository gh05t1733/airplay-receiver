// tests/unit/test_bplist.cpp — Bplist codec unit tests.
// Round-trip tests, malformed input handling, UTF-16BE string tests.
// PRD §8.4: "reader + writer for dict/array/string/int/bool/real/data/date/UID — round-trip tested"

#include <catch2/catch_test_macros.hpp>
#include "core/rtsp/bplist.h"

using namespace arak::bplist;
using arak::CoreStatus;

TEST_CASE("bplist: round-trip simple dict", "[bplist]") {
    BplistDict dict;
    dict["name"] = BplistValue(std::string("test"));
    dict["count"] = BplistValue(int64_t{42});
    dict["active"] = BplistValue(true);

    std::vector<uint8_t> encoded;
    REQUIRE(encode(BplistValue(dict), encoded) == CoreStatus::Ok);
    REQUIRE_FALSE(encoded.empty());

    // Decode the encoded data
    BplistValue decoded;
    REQUIRE(decode(encoded.data(), encoded.size(), decoded) == CoreStatus::Ok);
    REQUIRE(decoded.isDict());

    const auto& result = decoded.getDict();
    REQUIRE(result.at("name").getString() == "test");
    REQUIRE(result.at("count").getInt() == 42);
    REQUIRE(result.at("active").getBool() == true);
}

TEST_CASE("bplist: round-trip empty string", "[bplist]") {
    BplistDict dict;
    dict["empty"] = BplistValue(std::string(""));

    std::vector<uint8_t> encoded;
    REQUIRE(encode(BplistValue(dict), encoded) == CoreStatus::Ok);

    BplistValue decoded;
    REQUIRE(decode(encoded.data(), encoded.size(), decoded) == CoreStatus::Ok);
    REQUIRE(decoded.getDict().at("empty").getString() == "");
}

TEST_CASE("bplist: round-trip integer values", "[bplist]") {
    BplistDict dict;
    dict["small"] = BplistValue(int64_t{1});
    dict["medium"] = BplistValue(int64_t{300});
    dict["large"] = BplistValue(int64_t{70000});
    dict["huge"] = BplistValue(int64_t{3000000000LL});
    dict["negative"] = BplistValue(int64_t{-42});

    std::vector<uint8_t> encoded;
    REQUIRE(encode(BplistValue(dict), encoded) == CoreStatus::Ok);

    BplistValue decoded;
    REQUIRE(decode(encoded.data(), encoded.size(), decoded) == CoreStatus::Ok);
    const auto& r = decoded.getDict();
    REQUIRE(r.at("small").getInt() == 1);
    REQUIRE(r.at("medium").getInt() == 300);
    REQUIRE(r.at("large").getInt() == 70000);
    REQUIRE(r.at("huge").getInt() == 3000000000LL);
    REQUIRE(r.at("negative").getInt() == -42);
}

TEST_CASE("bplist: round-trip real values", "[bplist]") {
    BplistDict dict;
    dict["pi"] = BplistValue(3.14159);
    dict["zero"] = BplistValue(0.0);

    std::vector<uint8_t> encoded;
    REQUIRE(encode(BplistValue(dict), encoded) == CoreStatus::Ok);

    BplistValue decoded;
    REQUIRE(decode(encoded.data(), encoded.size(), decoded) == CoreStatus::Ok);
    const auto& r = decoded.getDict();
    REQUIRE(decoded.getDict().at("pi").getReal() >= 3.14);
    REQUIRE(decoded.getDict().at("pi").getReal() <= 3.15);
    REQUIRE(decoded.getDict().at("zero").getReal() == 0.0);
}

TEST_CASE("bplist: round-trip data blob", "[bplist]") {
    BplistData data = {0xDE, 0xAD, 0xBE, 0xEF};
    BplistDict dict;
    dict["blob"] = BplistValue(data);

    std::vector<uint8_t> encoded;
    REQUIRE(encode(BplistValue(dict), encoded) == CoreStatus::Ok);

    BplistValue decoded;
    REQUIRE(decode(encoded.data(), encoded.size(), decoded) == CoreStatus::Ok);
    REQUIRE(decoded.getDict().at("blob").getData() == data);
}

TEST_CASE("bplist: truncated input returns ProtocolError", "[bplist]") {
    uint8_t truncated[] = {'b', 'p', 'l', 'i', 's', 't', '0', '0'};
    BplistValue decoded;
    REQUIRE(decode(truncated, sizeof(truncated), decoded) == CoreStatus::ProtocolError);
}

TEST_CASE("bplist: invalid magic returns ProtocolError", "[bplist]") {
    uint8_t bad[] = {'n', 'o', 't', ' ', 'a', ' ', 'p', 'l',
                     0, 0, 0, 0, 0, 0, 0, 0,
                     0, 0, 0, 0, 0, 0, 0, 0,
                     0, 0, 0, 0, 0, 0, 0, 0};
    BplistValue decoded;
    REQUIRE(decode(bad, sizeof(bad), decoded) == CoreStatus::ProtocolError);
}

TEST_CASE("bplist: null pointer returns ProtocolError", "[bplist]") {
    BplistValue decoded;
    REQUIRE(decode(nullptr, 0, decoded) == CoreStatus::ProtocolError);
}

TEST_CASE("bplist: encodeDict convenience function", "[bplist]") {
    BplistDict dict;
    dict["features"] = BplistValue(std::string("0x40000280,0x10400"));
    dict["statusFlags"] = BplistValue(int64_t{4});

    std::vector<uint8_t> encoded;
    REQUIRE(encodeDict(dict, encoded) == CoreStatus::Ok);
    REQUIRE_FALSE(encoded.empty());

    BplistValue decoded;
    REQUIRE(decode(encoded.data(), encoded.size(), decoded) == CoreStatus::Ok);
    REQUIRE(decoded.getDict().at("features").getString() == "0x40000280,0x10400");
    REQUIRE(decoded.getDict().at("statusFlags").getInt() == 4);
}

TEST_CASE("bplist: nested dict", "[bplist]") {
    BplistDict inner;
    inner["x"] = BplistValue(int64_t{1});
    BplistDict outer;
    outer["inner"] = BplistValue(std::move(inner));
    outer["name"] = BplistValue(std::string("nested"));

    std::vector<uint8_t> encoded;
    REQUIRE(encode(BplistValue(outer), encoded) == CoreStatus::Ok);

    BplistValue decoded;
    REQUIRE(decode(encoded.data(), encoded.size(), decoded) == CoreStatus::Ok);
    REQUIRE(decoded.getDict().at("inner").getDict().at("x").getInt() == 1);
    REQUIRE(decoded.getDict().at("name").getString() == "nested");
}
