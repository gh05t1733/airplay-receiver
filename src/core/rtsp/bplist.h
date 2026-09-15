// src/core/rtsp/bplist.h — Minimal binary plist (bplist00) reader/writer.
// Architecture brief C3, PRD §8.4. Pure code, no external dependency.
// Supports: dict, array, string (ASCII + UTF-16BE), int (1/2/4/8 bytes),
//           bool, real (4/8 bytes), data, date, UID.
// All lengths bounds-checked; truncated/oversize input returns ProtocolError.
#pragma once

#include "common/types.h"
#include <cstdint>
#include <variant>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace arak::bplist {

// Bplist value types.
struct BplistNull {};
using BplistData = std::vector<uint8_t>;

struct BplistValue;
using BplistArray = std::vector<BplistValue>;
using BplistDict = std::unordered_map<std::string, BplistValue>;

struct BplistValue {
    using ValueType = std::variant<
        BplistNull,
        bool,
        int64_t,
        double,
        std::string,
        BplistData,
        BplistArray,
        BplistDict
    >;

    ValueType value;

    BplistValue() : value(BplistNull{}) {}
    BplistValue(bool v) : value(v) {}
    BplistValue(int64_t v) : value(v) {}
    BplistValue(double v) : value(v) {}
    BplistValue(const std::string& v) : value(v) {}
    BplistValue(std::string&& v) : value(std::move(v)) {}
    BplistValue(BplistData v) : value(std::move(v)) {}
    BplistValue(BplistArray v) : value(std::move(v)) {}
    BplistValue(BplistDict v) : value(std::move(v)) {}

    bool isNull() const { return std::holds_alternative<BplistNull>(value); }
    bool isBool() const { return std::holds_alternative<bool>(value); }
    bool isInt() const { return std::holds_alternative<int64_t>(value); }
    bool isReal() const { return std::holds_alternative<double>(value); }
    bool isString() const { return std::holds_alternative<std::string>(value); }
    bool isData() const { return std::holds_alternative<BplistData>(value); }
    bool isArray() const { return std::holds_alternative<BplistArray>(value); }
    bool isDict() const { return std::holds_alternative<BplistDict>(value); }

    // Accessors (throw std::bad_variant_access on type mismatch)
    const std::string& getString() const { return std::get<std::string>(value); }
    std::string& getString() { return std::get<std::string>(value); }
    const BplistDict& getDict() const { return std::get<BplistDict>(value); }
    const BplistArray& getArray() const { return std::get<BplistArray>(value); }
    int64_t getInt() const { return std::get<int64_t>(value); }
    bool getBool() const { return std::get<bool>(value); }
    double getReal() const { return std::get<double>(value); }
    const BplistData& getData() const { return std::get<BplistData>(value); }
};

// Parse a binary plist from a byte buffer. Returns ProtocolError on invalid data.
CoreStatus decode(const uint8_t* data, size_t size, BplistValue& out);

// Encode a BplistValue to binary plist format.
CoreStatus encode(const BplistValue& value, std::vector<uint8_t>& out);

// Convenience: encode a dict directly.
CoreStatus encodeDict(const BplistDict& dict, std::vector<uint8_t>& out);

}  // namespace arak::bplist
