// airplay-receiver — Sprint 1 entry point.
//
// Sprint 1 gate (brief §8): advertise _airplay._tcp + _raop._tcp, answer GET /info with a
// valid bplist, show the Win32/D3D11 window with the status overlay, and shut down cleanly.
// NOT in Sprint 1: a decoded video frame (that is gate Sprint 2) — see brief migration D3.
//
// Bootstrap seed: proves the exe builds and links arak_common + spdlog. @code-executor
// replaces the body with the real app wiring (advertiser, RTSP server, window).
//
// ENTRY POINT: src/CMakeLists.txt builds this target with the WIN32 subsystem (GUI app, per
// brief §2 C16), so the CRT entry must be wWinMain. A console-style `int main()` does NOT link
// here — measured: LNK2019 "unresolved external symbol WinMain referenced in function
// __scrt_common_main_seh". UNICODE/_UNICODE are defined globally in the root CMakeLists, so the
// wide entry point is the correct one.

#include <windows.h>

#include <spdlog/spdlog.h>

#include "common/version.h"

int WINAPI wWinMain(HINSTANCE /*instance*/, HINSTANCE /*prev_instance*/,
                    PWSTR /*cmd_line*/, int /*show_cmd*/) {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("{}", arak::common::buildBanner());
    spdlog::info("Sprint 1 bootstrap: advertise + GET /info + Win32/D3D11 window");
    spdlog::info("Video frame is gate Sprint 2 (brief D3) - not claimed here.");
    return 0;
}
