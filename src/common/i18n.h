// src/common/i18n.h — Internationalization loader.
// All user-visible strings go through i18n::get(), never hardcoded (D6, brief §5.1/§7).
// Loads from assets/i18n/<locale>.json at startup.
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <mutex>

namespace arak::i18n {

// Initialize the i18n system with the given locale directory.
// Returns true on success. Falls back to "en" if the requested locale is missing.
bool init(std::string_view localeDir, std::string_view locale = "en");

// Get a translated string by dot-separated key (e.g. "state.advertising.label").
// Falls back to the key itself if not found.
std::string get(std::string_view key);

// Get a translated string and replace {placeholder} tokens.
// e.g. get("metric.fps", {{"value", "60"}}) -> "fps 60"
std::string get(std::string_view key, const std::unordered_map<std::string, std::string>& params);

// Switch locale at runtime. Returns true on success.
bool switchLocale(std::string_view newLocale);

// Get current locale code.
std::string currentLocale();

}  // namespace arak::i18n
