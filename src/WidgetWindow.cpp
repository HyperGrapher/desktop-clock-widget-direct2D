#include "WidgetWindow.h"

#include "ClockMath.h"
#include "Win32Helpers.h"
#include "../resources/resource.h"

#include <shellapi.h>

#include <algorithm>
#include <string>

namespace widget {
namespace {

constexpr wchar_t kWindowClass[] = L"DesktopClockWidget.Window";
constexpr wchar_t kWindowTitle[] = L"Desktop Clock Widget";
constexpr float kWidgetSizeDip = 280.0f;
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT_PTR kMidnightTimer = 1;
constexpr UINT kCommandLock = 1001;
constexpr UINT kCommandTopmost = 1002;
constexpr UINT kCommandReset = 1003;
constexpr UINT kCommandExit = 1004;
constexpr UINT kTrayId = 1;

struct MonitorMatch {
    std::wstring desired;
    HMONITOR monitor = nullptr;
};

BOOL CALLBACK FindMonitorCallback(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto* match = reinterpret_cast<MonitorMatch*>(data);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info) && match->desired == info.szDevice) {
        match->monitor = monitor;
        return FALSE;
    }
    return TRUE;
}

HMONITOR FindSavedMonitor(const std::wstring& device) {
    if (!device.empty()) {
        MonitorMatch match{device};
        EnumDisplayMonitors(nullptr, nullptr, FindMonitorCallback,
                            reinterpret_cast<LPARAM>(&match));
        if (match.monitor) {
            return match.monitor;
        }
    }
    POINT origin{};
    return MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
}

MONITORINFOEXW GetMonitorDetails(HMONITOR monitor) {
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    CheckWin32(GetMonitorInfoW(monitor, &info), "GetMonitorInfoW");
    return info;
}

} // namespace

WidgetWindow::WidgetWindow(HINSTANCE instance) : instance_(instance) {}

WidgetWindow::~WidgetWindow() {
    RemoveTrayIcon();
}

void WidgetWindow::Create() {
    settings_ = settingsStore_.Load();

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_SIZEALL);
    windowClass.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_CLOCK));
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.lpszClassName = kWindowClass;
    CheckWin32(RegisterClassExW(&windowClass) != 0, "RegisterClassExW");

    const HMONITOR monitor = FindSavedMonitor(settings_.monitorDevice);
    const auto monitorInfo = GetMonitorDetails(monitor);
    const int provisionalSize = DipToPixels(kWidgetSizeDip, GetDpiForSystem());
    const bool hasSavedPosition = !settings_.monitorDevice.empty();
    const int initialX = hasSavedPosition
                             ? monitorInfo.rcWork.left + settings_.offsetX
                             : monitorInfo.rcWork.right - provisionalSize - settings_.offsetX;
    const int initialY = monitorInfo.rcWork.top + settings_.offsetY;

    DWORD extendedStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP;
    if (settings_.locked) {
        extendedStyle |= WS_EX_TRANSPARENT;
    }
    if (settings_.alwaysOnTop) {
        extendedStyle |= WS_EX_TOPMOST;
    }

    window_ = CreateWindowExW(extendedStyle, kWindowClass, kWindowTitle, WS_POPUP, initialX,
                              initialY, provisionalSize, provisionalSize, nullptr, nullptr,
                              instance_, this);
    if (!window_) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "CreateWindowExW");
    }

    dpi_ = GetDpiForWindow(window_);
    const int size = DipToPixels(kWidgetSizeDip, dpi_);
    SetWindowPos(window_, settings_.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, initialX,
                 initialY, size, size, SWP_NOACTIVATE);
    EnsureOnScreen();
    ApplyCircularRegion();
    InitializeGraphics();
    AddTrayIcon();
    ScheduleMidnightTimer();
    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
    ShowWindow(window_, SW_SHOWNOACTIVATE);
    UpdateWindow(window_);
}

int WidgetWindow::RunMessageLoop() {
    MSG message{};
    while (true) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == 0) {
            return static_cast<int>(message.wParam);
        }
        if (result == -1) {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                    "GetMessageW");
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

LRESULT CALLBACK WidgetWindow::WindowProc(HWND window, UINT message, WPARAM wParam,
                                          LPARAM lParam) noexcept {
    WidgetWindow* self = reinterpret_cast<WidgetWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<WidgetWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) {
        return DefWindowProcW(window, message, wParam, lParam);
    }

    try {
        return self->HandleMessage(message, wParam, lParam);
    } catch (const std::exception& error) {
        OutputDebugStringA(error.what());
        OutputDebugStringA("\n");
        MessageBoxA(window, error.what(), "Desktop Clock Widget", MB_OK | MB_ICONERROR);
        if (message != WM_DESTROY && IsWindow(window)) {
            DestroyWindow(window);
        }
        return 0;
    }
}

LRESULT WidgetWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    if (taskbarCreatedMessage_ != 0 && message == taskbarCreatedMessage_) {
        trayIconAdded_ = false;
        AddTrayIcon();
        return 0;
    }

    switch (message) {
    case WM_NCHITTEST:
        return settings_.locked ? HTTRANSPARENT : HTCAPTION;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case kTrayMessage:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU ||
            LOWORD(lParam) == WM_LBUTTONUP) {
            ShowTrayMenu();
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kCommandLock:
            SetLocked(!settings_.locked);
            break;
        case kCommandTopmost:
            SetAlwaysOnTop(!settings_.alwaysOnTop);
            break;
        case kCommandReset:
            ResetPosition();
            break;
        case kCommandExit:
            DestroyWindow(window_);
            break;
        default:
            break;
        }
        return 0;

    case WM_EXITSIZEMOVE:
        EnsureOnScreen();
        SaveSettings();
        return 0;

    case WM_DPICHANGED: {
        dpi_ = HIWORD(wParam);
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        const int size = DipToPixels(kWidgetSizeDip, dpi_);
        SetWindowPos(window_, nullptr, suggested->left, suggested->top, size, size,
                     SWP_NOACTIVATE | SWP_NOZORDER);
        ApplyCircularRegion();
        renderer_.Resize(dpi_, !settings_.locked, acrylic_.IsAvailable());
        SaveSettings();
        return 0;
    }

    case WM_DISPLAYCHANGE:
        EnsureOnScreen();
        SaveSettings();
        return 0;

    case WM_TIMECHANGE:
        RefreshDate();
        ResynchronizeHands();
        ScheduleMidnightTimer();
        return 0;

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            RefreshDate();
            ResynchronizeHands();
            ScheduleMidnightTimer();
        }
        return TRUE;

    case WM_DWMCOMPOSITIONCHANGED:
        InitializeGraphics();
        return 0;

    case WM_TIMER:
        if (wParam == kMidnightTimer) {
            RefreshDate();
            ResynchronizeHands();
            ScheduleMidnightTimer();
        }
        return 0;

    case WM_ENDSESSION:
        if (wParam) {
            SaveSettings();
        }
        return 0;

    case WM_DESTROY:
        KillTimer(window_, kMidnightTimer);
        SaveSettings();
        RemoveTrayIcon();
        renderer_.Reset();
        acrylic_.Reset();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(window_, message, wParam, lParam);
    }
}

void WidgetWindow::InitializeGraphics() {
    bool acrylicAvailable = false;
    try {
        acrylicAvailable = acrylic_.Initialize(window_);
    } catch (...) {
        acrylic_.Reset();
    }
    renderer_.Initialize(window_, dpi_, !settings_.locked, acrylicAvailable);
}

void WidgetWindow::RecreateGraphics() {
    renderer_.Initialize(window_, dpi_, !settings_.locked, acrylic_.IsAvailable());
}

void WidgetWindow::ApplyCircularRegion() {
    RECT client{};
    CheckWin32(GetClientRect(window_, &client), "GetClientRect");
    HRGN region = CreateEllipticRgn(client.left, client.top, client.right + 1, client.bottom + 1);
    if (!region) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "CreateEllipticRgn");
    }
    if (SetWindowRgn(window_, region, TRUE) == 0) {
        DeleteObject(region);
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "SetWindowRgn");
    }
}

void WidgetWindow::AddTrayIcon() {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window_;
    data.uID = kTrayId;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = kTrayMessage;
    data.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_CLOCK));
    wcscpy_s(data.szTip, kWindowTitle);
    CheckWin32(Shell_NotifyIconW(NIM_ADD, &data), "Shell_NotifyIconW(NIM_ADD)");
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    trayIconAdded_ = true;
}

void WidgetWindow::RemoveTrayIcon() noexcept {
    if (!trayIconAdded_ || !window_) {
        return;
    }
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window_;
    data.uID = kTrayId;
    Shell_NotifyIconW(NIM_DELETE, &data);
    trayIconAdded_ = false;
}

void WidgetWindow::ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "CreatePopupMenu");
    }
    AppendMenuW(menu, MF_STRING | (settings_.locked ? MF_CHECKED : 0), kCommandLock,
                L"Click-through / locked");
    AppendMenuW(menu, MF_STRING | (settings_.alwaysOnTop ? MF_CHECKED : 0), kCommandTopmost,
                L"Always on top");
    AppendMenuW(menu, MF_STRING, kCommandReset, L"Reset position");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCommandExit, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursor.x, cursor.y,
                   0, window_, nullptr);
    PostMessageW(window_, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

void WidgetWindow::SetLocked(bool locked) {
    settings_.locked = locked;
    LONG_PTR style = GetWindowLongPtrW(window_, GWL_EXSTYLE);
    if (locked) {
        style |= WS_EX_TRANSPARENT;
    } else {
        style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    }
    SetWindowLongPtrW(window_, GWL_EXSTYLE, style);
    SetWindowPos(window_, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    renderer_.SetUnlocked(!locked);
    acrylic_.SetInputActive(true);
    SaveSettings();
}

void WidgetWindow::SetAlwaysOnTop(bool alwaysOnTop) {
    settings_.alwaysOnTop = alwaysOnTop;
    SetWindowPos(window_, alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SaveSettings();
}

void WidgetWindow::ResetPosition() {
    POINT origin{};
    const auto info = GetMonitorDetails(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY));
    RECT windowRect{};
    GetWindowRect(window_, &windowRect);
    const int width = windowRect.right - windowRect.left;
    const int x = info.rcWork.right - width - DipToPixels(32.0f, dpi_);
    const int y = info.rcWork.top + DipToPixels(32.0f, dpi_);
    SetWindowPos(window_, nullptr, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    SaveSettings();
}

void WidgetWindow::EnsureOnScreen() {
    RECT rect{};
    GetWindowRect(window_, &rect);
    const HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    const auto info = GetMonitorDetails(monitor);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int x = std::clamp(rect.left, info.rcWork.left, std::max(info.rcWork.left,
                                                                  info.rcWork.right - width));
    const int y = std::clamp(rect.top, info.rcWork.top, std::max(info.rcWork.top,
                                                                 info.rcWork.bottom - height));
    if (x != rect.left || y != rect.top) {
        SetWindowPos(window_, nullptr, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void WidgetWindow::SaveSettings() {
    if (!window_) {
        return;
    }
    RECT rect{};
    GetWindowRect(window_, &rect);
    const auto info = GetMonitorDetails(MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST));
    settings_.monitorDevice = info.szDevice;
    settings_.offsetX = rect.left - info.rcWork.left;
    settings_.offsetY = rect.top - info.rcWork.top;
    settingsStore_.Save(settings_);
}

void WidgetWindow::ScheduleMidnightTimer() {
    KillTimer(window_, kMidnightTimer);
    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);
    const UINT delay = MillisecondsUntilNextLocalMidnight(localTime);
    if (SetTimer(window_, kMidnightTimer, delay, nullptr) == 0) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "SetTimer");
    }
}

void WidgetWindow::RefreshDate() {
    try {
        renderer_.RefreshDate();
    } catch (...) {
        RecreateGraphics();
    }
}

void WidgetWindow::ResynchronizeHands() {
    try {
        renderer_.ResynchronizeHands();
    } catch (...) {
        RecreateGraphics();
    }
}

} // namespace widget
