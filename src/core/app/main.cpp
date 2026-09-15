// airplay-receiver — Sprint 1 entry point.
//
// Sprint 1 gate (brief §8): advertise _airplay._tcp + _raop._tcp, answer GET /info with a
// valid bplist, show the Win32 window with the status overlay, and shut down cleanly.
// NOT in Sprint 1: a decoded video frame (that is gate Sprint 2) — see brief migration D3.
//
// Thread inventory (brief §4.1):
//   T1 (mdns)     — mDNS registration (daemon-owned, our process does not own 5353)
//   T2 (rtsp-accept) — TCP accept loop (inside RtspServer)
//   T9 (ui)       — Win32 message pump (the main thread after startup)
//
// Shutdown order (brief §4.4):
//   1. Stop accepting new connections
//   2. Close existing sessions
//   3. Stop mDNS advertisement
//   4. Destroy window
//   5. Exit

#include <windows.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

#include "common/version.h"
#include "common/types.h"
#include "common/i18n.h"
#include "common/identity.h"
#include "core/rtsp/rtsp_server.h"
#include "core/discovery/advertiser.h"
#include "ui/win32/window.h"

#include <filesystem>
#include <string>
#include <memory>
#include <csignal>

namespace fs = std::filesystem;

// Global shutdown flag (set by signal handler or window close).
static std::atomic<bool> g_shutdownRequested{false};

static void signalHandler(int) {
    g_shutdownRequested = true;
}

// Get the application data directory: %LOCALAPPDATA%\Arakatian\AirPlayReceiver
static std::string getLocalDataDir() {
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (!localAppData) localAppData = ".";
    return std::string(localAppData) + "\\Arakatian\\AirPlayReceiver";
}

// Get the app data directory: %APPDATA%\Arakatian\AirPlayReceiver
static std::string getAppDataDir() {
    const char* appData = std::getenv("APPDATA");
    if (!appData) appData = ".";
    return std::string(appData) + "\\Arakatian\\AirPlayReceiver";
}

// Set up spdlog with rotating file + console sinks.
static void setupLogging(const std::string& logDir) {
    fs::create_directories(logDir);

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_sink_st>();
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_st>(
        logDir + "\\receiver.log",
        1024 * 1024,   // max size 1 MB
        3,              // keep 3 rotated files
        false           // don't truncate on open
    );

    auto logger = std::make_shared<spdlog::logger>(
        "airplay", spdlog::sinks_init_list{consoleSink, fileSink});
    logger->set_level(spdlog::level::info);
    spdlog::set_default_logger(logger);
}

int WINAPI wWinMain(HINSTANCE /*instance*/, HINSTANCE /*prev_instance*/,
                    PWSTR /*cmd_line*/, int /*show_cmd*/) {

    // Register signal handlers for clean shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // --- Stage 1: Logging ---
    auto logDir = getLocalDataDir() + "\\logs";
    setupLogging(logDir);
    spdlog::info("{}", arak::common::buildBanner());
    spdlog::info("Sprint 1: advertise + GET /info + Win32 window");

    // --- Stage 2: i18n ---
    // Locate assets relative to the executable (or use a default path)
    // For development, assets live in the source tree
    std::string i18nDir = "assets/i18n";
    // Try to find it relative to the exe
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH)) {
        fs::path exeDir = fs::path(exePath).parent_path();
        auto candidate = exeDir / "assets" / "i18n";
        if (fs::exists(candidate)) i18nDir = candidate.string();
    }
    arak::i18n::init(i18nDir, "en");
    spdlog::info("i18n: locale = {}", arak::i18n::currentLocale());

    // --- Stage 3: Identity ---
    arak::identity::ReceiverIdentity identity;
    identity.name = arak::i18n::get("app.title");
    if (identity.name == "app.title") identity.name = "AirPlay Receiver";  // fallback

    auto dataDir = getLocalDataDir();
    if (!arak::identity::initIdentity(dataDir + "\\identity", identity)) {
        spdlog::error("Failed to initialize identity");
        return 1;
    }
    spdlog::info("identity: device={}, pk={}", identity.deviceId,
                 identity.publicKeyHex.substr(0, 8) + "...");
    spdlog::info("identity: features={}", arak::identity::encodeFeatures(identity.features));

    // --- Stage 4: RTSP Server ---
    arak::rtsp::RtspServer rtspServer;
    arak::rtsp::RtspConfig rtspConfig;
    rtspConfig.port = identity.rtspPort;

    auto rtspStatus = rtspServer.start(rtspConfig, identity);
    if (rtspStatus != arak::CoreStatus::Ok) {
        spdlog::error("RTSP server failed to start: {}", arak::coreStatusToString(rtspStatus));
        spdlog::error("Port {} may be in use. Check the log for details.", rtspConfig.port);
        return 1;
    }
    // Update identity with the actual bound port
    identity.rtspPort = rtspServer.boundPort();
    spdlog::info("RTSP: listening on port {}", identity.rtspPort);

    // --- Stage 5: mDNS Advertisement ---
    arak::discovery::Advertiser advertiser;
    auto advStatus = advertiser.start(identity, identity.rtspPort);
    if (advStatus != arak::CoreStatus::Ok) {
        spdlog::error("mDNS advertiser failed to start: {}", arak::coreStatusToString(advStatus));
        // Non-fatal: the app can still run without mDNS
    }
    spdlog::info("mDNS: using daemon = {}", advertiser.usingDaemon() ? "yes" : "log-only");

    // Log advertised addresses
    for (const auto& addr : advertiser.localAddresses()) {
        spdlog::info("mDNS: address {}", addr);
    }

    // --- Stage 6: Window ---
    arak::ui::Window window;
    arak::ui::WindowConfig winConfig;
    winConfig.title = arak::i18n::get("app.title");

    auto winStatus = window.create(winConfig);
    if (winStatus != arak::CoreStatus::Ok) {
        spdlog::error("Window creation failed: {}", arak::coreStatusToString(winStatus));
        return 1;
    }

    // Update window status to "advertising"
    window.setStatus(arak::i18n::get("state.advertising.label"));
    window.setDetail(arak::i18n::get("state.advertising.detail"));

    // --- Stage 7: Message Pump (T9) ---
    spdlog::info("App started — waiting for connections");
    int exitCode = window.run();

    // --- Stage 8: Shutdown (brief §4.4) ---
    spdlog::info("Shutting down...");

    // 1. Stop mDNS (deregister services)
    advertiser.stop();

    // 2. Stop RTSP server (close all connections)
    rtspServer.stop();

    spdlog::info("Shutdown complete");
    spdlog::drop_all();

    return exitCode;
}
