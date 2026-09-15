// src/common/i18n.cpp — JSON-based i18n loader implementation.
// Reads assets/i18n/<locale>.json, provides dot-key lookup with placeholder replacement.
// Falls back to English if a key is missing; logs the fallback once.

#include "common/i18n.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_set>
#include <spdlog/spdlog.h>

// Minimal JSON parser for flat/nested string-only values.
// We only need to extract string values from the locale files — no arrays, no numbers.
namespace {

// Simple JSON value: either a string or a map of keys to values.
struct JsonValue {
    std::string strValue;
    std::unordered_map<std::string, JsonValue> mapValue;
    bool isMap = false;
};

bool parseJsonValue(const std::string& json, size_t& pos, JsonValue& result);

bool parseString(const std::string& json, size_t& pos, std::string& out) {
    if (pos >= json.size() || json[pos] != '"') return false;
    ++pos;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            switch (json[pos]) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            default: out += json[pos]; break;
            }
        } else {
            out += json[pos];
        }
        ++pos;
    }
    if (pos >= json.size()) return false;
    ++pos;  // skip closing quote
    return true;
}

void skipWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\n' || json[pos] == '\r')) {
        ++pos;
    }
}

bool parseObject(const std::string& json, size_t& pos, JsonValue& result) {
    if (pos >= json.size() || json[pos] != '{') return false;
    ++pos;
    result.isMap = true;
    skipWhitespace(json, pos);
    if (pos < json.size() && json[pos] == '}') { ++pos; return true; }
    while (pos < json.size()) {
        skipWhitespace(json, pos);
        std::string key;
        if (!parseString(json, pos, key)) return false;
        skipWhitespace(json, pos);
        if (pos >= json.size() || json[pos] != ':') return false;
        ++pos;
        skipWhitespace(json, pos);
        JsonValue val;
        if (!parseJsonValue(json, pos, val)) return false;
        result.mapValue[std::move(key)] = std::move(val);
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ',') { ++pos; continue; }
        break;
    }
    skipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '}') return false;
    ++pos;
    return true;
}

bool parseJsonValue(const std::string& json, size_t& pos, JsonValue& result) {
    skipWhitespace(json, pos);
    if (pos >= json.size()) return false;
    if (json[pos] == '"') {
        result.isMap = false;
        return parseString(json, pos, result.strValue);
    }
    if (json[pos] == '{') {
        return parseObject(json, pos, result);
    }
    // Skip non-string values (numbers, bools, null) — consume until next delimiter
    size_t start = pos;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ']') {
        ++pos;
    }
    result.isMap = false;
    result.strValue = json.substr(start, pos - start);
    return true;
}

// Look up a dot-separated key in the parsed JSON tree.
bool lookupKey(const JsonValue& root, std::string_view key, std::string& out) {
    size_t dotPos = key.find('.');
    if (dotPos != std::string_view::npos) {
        std::string_view first = key.substr(0, dotPos);
        auto it = root.mapValue.find(std::string(first));
        if (it == root.mapValue.end()) return false;
        return lookupKey(it->second, key.substr(dotPos + 1), out);
    }
    auto it = root.mapValue.find(std::string(key));
    if (it == root.mapValue.end()) return false;
    if (it->second.isMap) return false;
    out = it->second.strValue;
    return true;
}

// Replace {placeholder} tokens in a template string.
std::string replacePlaceholders(std::string_view tmpl,
                                const std::unordered_map<std::string, std::string>& params) {
    std::string result;
    result.reserve(tmpl.size());
    size_t i = 0;
    while (i < tmpl.size()) {
        size_t brace = tmpl.find('{', i);
        if (brace == std::string_view::npos) {
            result.append(tmpl.substr(i));
            break;
        }
        result.append(tmpl.substr(i, brace - i));
        size_t close = tmpl.find('}', brace);
        if (close == std::string_view::npos) {
            result += '{';
            ++i;
            continue;
        }
        std::string_view placeholder = tmpl.substr(brace + 1, close - brace - 1);
        auto it = params.find(std::string(placeholder));
        if (it != params.end()) {
            result += it->second;
        } else {
            result += '{';
            result += placeholder;
            result += '}';
        }
        i = close + 1;
    }
    return result;
}

// Singleton state
struct I18nState {
    std::mutex mu;
    std::string localeDir;
    std::string currentLocale;
    JsonValue root;
    bool initialized = false;
    std::unordered_set<std::string> loggedFallbacks;  // track which keys we've warned about
};

I18nState& getState() {
    static I18nState state;
    return state;
}

bool loadLocale(I18nState& state, std::string_view locale) {
    auto path = std::filesystem::path(state.localeDir) / (std::string(locale) + ".json");
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    size_t pos = 0;
    JsonValue root;
    if (!parseJsonValue(content, pos, root) || !root.isMap) {
        spdlog::error("i18n: failed to parse {}", path.string());
        return false;
    }
    state.root = std::move(root);
    state.currentLocale = std::string(locale);
    return true;
}

}  // anonymous namespace

namespace arak::i18n {

bool init(std::string_view localeDir, std::string_view locale) {
    auto& state = getState();
    std::lock_guard lock(state.mu);
    state.localeDir = std::string(localeDir);
    state.loggedFallbacks.clear();

    // Try requested locale, fall back to "en"
    if (!loadLocale(state, locale)) {
        spdlog::warn("i18n: locale '{}' not found, falling back to 'en'", locale);
        if (!loadLocale(state, "en")) {
            spdlog::error("i18n: failed to load 'en' locale from {}", state.localeDir);
            state.initialized = false;
            return false;
        }
    }
    state.initialized = true;
    spdlog::info("i18n: loaded locale '{}' from {}", state.currentLocale, state.localeDir);
    return true;
}

std::string get(std::string_view key) {
    auto& state = getState();
    std::lock_guard lock(state.mu);
    if (!state.initialized) return std::string(key);
    std::string val;
    if (lookupKey(state.root, key, val)) return val;
    // Fallback: try English if we're not already in English
    if (state.currentLocale != "en") {
        I18nState enState;
        enState.localeDir = state.localeDir;
        if (loadLocale(enState, "en")) {
            if (lookupKey(enState.root, key, val)) return val;
        }
    }
    // Log fallback once per key
    if (state.loggedFallbacks.find(std::string(key)) == state.loggedFallbacks.end()) {
        spdlog::warn("i18n: key '{}' not found in locale '{}'", key, state.currentLocale);
        state.loggedFallbacks.insert(std::string(key));
    }
    return std::string(key);
}

std::string get(std::string_view key, const std::unordered_map<std::string, std::string>& params) {
    return replacePlaceholders(get(key), params);
}

bool switchLocale(std::string_view newLocale) {
    auto& state = getState();
    std::lock_guard lock(state.mu);
    state.loggedFallbacks.clear();
    if (!loadLocale(state, newLocale)) {
        spdlog::warn("i18n: switch to '{}' failed, falling back to 'en'", newLocale);
        if (!loadLocale(state, "en")) return false;
    }
    spdlog::info("i18n: switched to locale '{}'", state.currentLocale);
    return true;
}

std::string currentLocale() {
    auto& state = getState();
    std::lock_guard lock(state.mu);
    return state.currentLocale;
}

}  // namespace arak::i18n
