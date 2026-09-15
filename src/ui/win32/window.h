// src/ui/win32/window.h — Win32 window for the AirPlay receiver.
// Architecture brief C16, PRD §8.5, gate 1.6.
// Creates a window with a dark background + status/FPS overlay.
// ESC / Alt+F4 quits cleanly.
#pragma once

#include "common/types.h"
#include <cstdint>
#include <string>
#include <functional>

namespace arak::ui {

// Render statistics (brief §5.5 RenderStats).
struct RenderStats {
    double fps = 0.0;
    uint64_t presented = 0;
    uint64_t dropped = 0;
    int64_t presentLatencyUs = 0;
};

// Window configuration.
struct WindowConfig {
    int width = 1280;
    int height = 720;
    std::string title = "AirPlay Receiver";
};

// Callback for window state changes.
using WindowCallback = std::function<void()>;

class Window {
public:
    Window();
    ~Window();

    // Create and show the window.
    CoreStatus create(const WindowConfig& config);

    // Enter the message pump (blocks until WM_QUIT).
    // This is called on the main thread (T9).
    int run();

    // Request the window to close (signals WM_QUIT).
    void requestClose();

    // Get the HWND (for D3D11 device creation).
    void* hwnd() const;

    // Get render statistics.
    RenderStats stats() const;

    // Update the status text shown in the overlay.
    void setStatus(const std::string& status);

    // Update the detail text shown in the overlay.
    void setDetail(const std::string& detail);

    // Update metric text.
    void setMetric(const std::string& metric);

    // Set a quit callback.
    void onQuit(WindowCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace arak::ui
