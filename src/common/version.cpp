#include "common/version.h"

#include <string>

namespace arak::common {

std::string productName() { return "airplay-receiver"; }

std::string versionString() {
#ifdef ARAK_VERSION
    return ARAK_VERSION;
#else
    return "0.0.0-unknown";
#endif
}

std::string buildBanner() {
    return productName() + " " + versionString() + " (C++20)";
}

}  // namespace arak::common
