// src/ui/win32/window.cpp — Win32 window implementation.
// Creates a dark-background window with GDI text overlay for status/FPS.
// Architecture brief C16, gate 1.6. Presents at display refresh rate via a timer.

#include "window.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <spdlog/spdlog.h>

#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <mutex>

namespace arak::ui {

// Window class name
static const wchar_t kWindowClass[] = L"AirPlayReceiverWindow";

struct Window::Impl {
    HWND hwnd = nullptr;
    HINSTANCE hInstance = nullptr;
    std::atomic<bool> running{false};
    std::atomic<bool> closeRequested{false};
    WindowCallback quitCallback;

    // Status overlay text
    std::mutex textMutex;
    std::string statusText = "STARTING";
    std::string detailText = "preparing network advertisement…";
    std::string metricText;

    // Render stats
    std::atomic<uint64_t> presented{0};
    std::atomic<uint64_t> dropped{0};
    std::atomic<double> fps{0.0};
    std::chrono::steady_clock::time_point lastFpsCalc;
    uint64_t frameCount = 0;

    // Timer ID for the present loop
    static constexpr UINT_PTR TIMER_PRESENT = 1;
    static constexpr UINT PRESENT_INTERVAL_MS = 16;  // ~60 Hz

    // Paint the window with background + overlay text.
    void paint(HWND windowHwnd) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(windowHwnd, &ps);

        // Get window dimensions
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Dark background
        HBRUSH bgBrush = CreateSolidBrush(RGB(0x10, 0x12, 0x16));
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        // Set up text rendering
        SetBkMode(hdc, TRANSPARENT);

        // Status label (top-left, large)
        {
            std::lock_guard lock(textMutex);
            HFONT hFont = CreateFontW(
                -MulDiv(20, GetDeviceCaps(hdc, LOGPIXELSY), 72), 0, 0, 0,
                FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HFONT hOldFont = static_cast<HFONT>(SelectObject(hdc, hFont));

            // Determine state colour
            COLORREF stateColor = RGB(0x3D, 0xD6, 0x8C);  // green (ok)
            if (statusText == "STARTING") stateColor = RGB(0x58, 0xA6, 0xFF);  // blue (info)
            else if (statusText == "FAILED") stateColor = RGB(0xF0, 0x4E, 0x4E);  // red
            else if (statusText == "LOW FPS") stateColor = RGB(0xF5, 0xA5, 0x24);  // amber

            SetTextColor(hdc, stateColor);
            TextOutW(hdc, 16, 16,
                std::wstring(statusText.begin(), statusText.end()).c_str(),
                static_cast<int>(statusText.size()));

            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
        }

        // Detail text (below status)
        {
            std::lock_guard lock(textMutex);
            HFONT hFont = CreateFontW(
                -MulDiv(15, GetDeviceCaps(hdc, LOGPIXELSY), 72), 0, 0, 0,
                FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HFONT hOldFont = static_cast<HFONT>(SelectObject(hdc, hFont));
            SetTextColor(hdc, RGB(0xF2, 0xF4, 0xF7));  // white-ish
            TextOutW(hdc, 16, 48,
                std::wstring(detailText.begin(), detailText.end()).c_str(),
                static_cast<int>(detailText.size()));
            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
        }

        // Metrics (bottom-right, monospace)
        if (!metricText.empty()) {
            HFONT hFont = CreateFontW(
                -MulDiv(13, GetDeviceCaps(hdc, LOGPIXELSY), 72), 0, 0, 0,
                FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
            HFONT hOldFont = static_cast<HFONT>(SelectObject(hdc, hFont));
            SetTextColor(hdc, RGB(0xA8, 0xB0, 0xBD));  // muted

            SIZE textSize;
            GetTextExtentPoint32A(hdc, metricText.c_str(), static_cast<int>(metricText.size()), &textSize);
            TextOutA(hdc, rc.right - textSize.cx - 16, rc.bottom - textSize.cy - 16,
                metricText.c_str(), static_cast<int>(metricText.size()));

            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
        }

        // Hint text (bottom, centered)
        {
            HFONT hFont = CreateFontW(
                -MulDiv(11, GetDeviceCaps(hdc, LOGPIXELSY), 72), 0, 0, 0,
                FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HFONT hOldFont = static_cast<HFONT>(SelectObject(hdc, hFont));
            SetTextColor(hdc, RGB(0x7A, 0x82, 0x8F));  // muted
            const char* hint = "ESC / Alt+F4 = quit";
            SIZE textSize;
            GetTextExtentPoint32A(hdc, hint, static_cast<int>(strlen(hint)), &textSize);
            TextOutA(hdc, (rc.right - textSize.cx) / 2, rc.bottom - textSize.cy - 8,
                hint, static_cast<int>(strlen(hint)));
            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
        }

        EndPaint(windowHwnd, &ps);
    }

    // Timer callback for the present loop (~60 Hz).
    static void CALLBACK timerProc(HWND hwnd, UINT, UINT_PTR, DWORD) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (!self || !self->running) return;

        // Present = repaint the window
        InvalidateRect(hwnd, nullptr, FALSE);

        // Update FPS counter
        self->presented++;
        self->frameCount++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - self->lastFpsCalc).count();
        if (elapsed >= 1000) {
            self->fps = static_cast<double>(self->frameCount) * 1000.0 / elapsed;
            self->frameCount = 0;
            self->lastFpsCalc = now;
        }
    }

    // Window procedure.
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return 0;
        }

        case WM_PAINT:
            if (self) self->paint(hwnd);
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                if (self) self->closeRequested = true;
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (self) {
                self->running = false;
                KillTimer(hwnd, TIMER_PRESENT);
                if (self->quitCallback) self->quitCallback();
            }
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
};

Window::Window() : impl_(std::make_unique<Impl>()) {}
Window::~Window() { requestClose(); }

CoreStatus Window::create(const WindowConfig& config) {
    impl_->hInstance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &Impl::wndProc;
    wc.hInstance = impl_->hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;

    if (!RegisterClassExW(&wc)) {
        spdlog::error("Window: RegisterClassExW failed: {}", GetLastError());
        return CoreStatus::IoError;
    }

    // Calculate window size to account for borders
    RECT rc = {0, 0, config.width, config.height};
    DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&rc, style, FALSE);

    // Center on screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - (rc.right - rc.left)) / 2;
    int y = (screenH - (rc.bottom - rc.top)) / 2;

    // Convert title to wide string
    std::wstring wTitle(config.title.begin(), config.title.end());

    impl_->hwnd = CreateWindowExW(
        0, kWindowClass, wTitle.c_str(),
        style,
        x, y, rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, impl_->hInstance, impl_.get());

    if (!impl_->hwnd) {
        spdlog::error("Window: CreateWindowExW failed: {}", GetLastError());
        return CoreStatus::IoError;
    }

    ShowWindow(impl_->hwnd, SW_SHOW);
    UpdateWindow(impl_->hwnd);

    // Start the present timer (~60 Hz)
    impl_->lastFpsCalc = std::chrono::steady_clock::now();
    SetTimer(impl_->hwnd, Impl::TIMER_PRESENT, Impl::PRESENT_INTERVAL_MS, &Impl::timerProc);

    impl_->running = true;
    spdlog::info("Window: created {}x{} window", config.width, config.height);
    return CoreStatus::Ok;
}

int Window::run() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void Window::requestClose() {
    if (impl_->hwnd && impl_->running) {
        PostMessageW(impl_->hwnd, WM_CLOSE, 0, 0);
    }
}

void* Window::hwnd() const { return impl_->hwnd; }

RenderStats Window::stats() const {
    RenderStats s;
    s.fps = impl_->fps;
    s.presented = impl_->presented;
    s.dropped = impl_->dropped;
    return s;
}

void Window::setStatus(const std::string& status) {
    std::lock_guard lock(impl_->textMutex);
    impl_->statusText = status;
}

void Window::setDetail(const std::string& detail) {
    std::lock_guard lock(impl_->textMutex);
    impl_->detailText = detail;
}

void Window::setMetric(const std::string& metric) {
    std::lock_guard lock(impl_->textMutex);
    impl_->metricText = metric;
}

void Window::onQuit(WindowCallback cb) { impl_->quitCallback = std::move(cb); }

}  // namespace arak::ui
