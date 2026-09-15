# arak_common: status, logging, config, i18n loader, identity.
# Sprint 1: core types, i18n loader, receiver identity (Ed25519 keypair).
find_package(spdlog CONFIG REQUIRED)   # CI caught the missing import
# identity.cpp builds the Ed25519 keypair with OpenSSL (gate 1.12). Without this
# find_package the OpenSSL include dir is never on the compiler path (vcpkg adds NO
# global include dirs), so __has_include(<openssl/evp.h>) is false and identity.cpp
# silently compiles its PLACEHOLDER random-key branch - a green build advertising a
# pk that is not Ed25519 (R14). Measured, not assumed.
find_package(OpenSSL REQUIRED)

add_library(arak_common STATIC
    common/version.cpp
    common/i18n.cpp
    common/identity.cpp
)
target_include_directories(arak_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(arak_common PUBLIC spdlog::spdlog OpenSSL::Crypto)
target_compile_definitions(arak_common PUBLIC ARAK_VERSION="${PROJECT_VERSION}")

# arak_rtsp: RTSP server + bplist codec.
# Sprint 1: TCP listener, request parser, GET /info, documented errors.
add_library(arak_rtsp STATIC
    core/rtsp/bplist.cpp
    core/rtsp/rtsp_server.cpp
)
target_include_directories(arak_rtsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(arak_rtsp PUBLIC arak_common ws2_32)

# arak_discovery: mDNS advertiser via Bonjour daemon.
# Sprint 1: register _airplay._tcp + _raop._tcp through dnssd.dll.
add_library(arak_discovery STATIC
    core/discovery/advertiser.cpp
)
target_include_directories(arak_discovery PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(arak_discovery PUBLIC arak_common ws2_32)

# ui_win32: Win32 window + status overlay.
# Sprint 1: dark-background window with GDI text overlay, ESC/Alt+F4 quit.
add_library(ui_win32 STATIC
    ui/win32/window.cpp
)
target_include_directories(ui_win32 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ui_win32 PUBLIC arak_common)

# airplay-receiver: Sprint 1 window + advertisement + RTSP land here (brief §6.2).
if(MSVC)
    set(ARAK_EXE_SUBSYSTEM WIN32)
endif()

add_executable(airplay-receiver ${ARAK_EXE_SUBSYSTEM}
    core/app/main.cpp
)
target_link_libraries(airplay-receiver PRIVATE
    arak_common
    arak_rtsp
    arak_discovery
    ui_win32
    ws2_32
)
