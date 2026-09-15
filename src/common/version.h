// arak_common / version.h — build identity for the receiver.
// Sprint 1 bootstrap seed; extended by @code-executor (status, logger, config, i18n).
#pragma once

#include <string>

namespace arak::common {

// Product name shown in UI and logs.
std::string productName();

// Semantic version of the build (from CMake project version).
std::string versionString();

// Single-line build banner, e.g. "airplay-receiver 0.1.0 (C++20)".
std::string buildBanner();

}  // namespace arak::common
