#pragma once

#include <windows.h>

#include "AcrylicHost.h"
#include "CompositionRenderer.h"
#include "Settings.h"

namespace widget {

class WidgetWindow {
public:
    explicit WidgetWindow(HINSTANCE instance);
    ~WidgetWindow();

    WidgetWindow(const WidgetWindow&) = delete;
    WidgetWindow& operator=(const WidgetWindow&) = delete;

    void Create();
    int RunMessageLoop();

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam,
                                       LPARAM lParam) noexcept;
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void InitializeGraphics();
    void RecreateGraphics();
    void ApplyCircularRegion();
    void AddTrayIcon();
    void RemoveTrayIcon() noexcept;
    void ShowTrayMenu();
    void SetLocked(bool locked);
    void SetAlwaysOnTop(bool alwaysOnTop);
    void ResetPosition();
    void EnsureOnScreen();
    void SaveSettings();
    void ScheduleMidnightTimer();
    void RefreshDate();
    void ResynchronizeHands();

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    UINT dpi_ = 96;
    UINT taskbarCreatedMessage_ = 0;
    SettingsStore settingsStore_;
    WidgetSettings settings_;
    AcrylicHost acrylic_;
    CompositionRenderer renderer_;
    bool trayIconAdded_ = false;
};

} // namespace widget
