cmake_minimum_required(VERSION 3.25)

project(airplay-receiver
    VERSION 0.1.0
    DESCRIPTION "AirPlay HD receiver for Windows (Sprint 1: advertise + GET /info + Win32/D3D11 window)"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global settings (brief §6.1: C++20, MSVC /W4 /permissive-)
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Generator-agnostic binary output (brief 6.2 / v3.6).
# Without this, add_subdirectory(src) scatters binaries into per-target, per-config
# paths - VS multi-config lands the exe in build/src/Release/, Ninja single-config in
# build/src/ - so ANY fixed upload path is wrong for one generator. Measured cost:
# two consecutive *successful* CI runs uploaded nothing because of exactly this, and
# the repo's total artifact count stayed 0 (runs 35013172955, 35013648139).
# Pinning it here means CI can hardcode build/bin/*.exe for every generator and config.
# Side benefit: VCPKG_APPLOCAL_DEPS drops spdlog.dll/libcrypto-3-x64.dll next to the
# exe, which is what the Sprint 4 Inno Setup payload needs.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}/bin")
endforeach()

# Single-config generators (Ninja) need an explicit build type; multi-config (VS) ignore it.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()

# ---------------------------------------------------------------------------
# Options (brief §6.1/§6.2)
# ---------------------------------------------------------------------------
option(BUILD_TESTING "Build unit tests (Catch2)" ON)
option(BUILD_WPF_UI "Build the optional WPF client (requires the .NET 8 SDK)" OFF)

# FFmpeg is a vcpkg *manifest feature*, not a hard dependency (brief v3.2): Sprint 1 never
# touches media, so configure/build/test must succeed without it. Turn it on with
#   -DVCPKG_MANIFEST_FEATURES=video  -DAIRPLAY_ENABLE_FFMPEG=ON
option(AIRPLAY_ENABLE_FFMPEG "Link the FFmpeg media stack (Sprint 2+; needs VCPKG_MANIFEST_FEATURES=video)" OFF)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
add_subdirectory(src)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "airplay-receiver ${PROJECT_VERSION}")
message(STATUS "  generator      : ${CMAKE_GENERATOR}")
message(STATUS "  config types   : ${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "  BUILD_TESTING  : ${BUILD_TESTING}")
message(STATUS "  BUILD_WPF_UI   : ${BUILD_WPF_UI}")
