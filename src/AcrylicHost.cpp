#include "AcrylicHost.h"

#include "Win32Helpers.h"

#include <DispatcherQueue.h>
#include <Windows.UI.Composition.Interop.h>
#include <dwmapi.h>

#include <winrt/Microsoft.UI.h>

namespace {

winrt::Microsoft::UI::WindowId WindowIdFromWindow(HWND window) {
    const HMODULE frameworkUdk = LoadLibraryW(L"Microsoft.Internal.FrameworkUdk.dll");
    if (!frameworkUdk) {
        widget::CheckHr(HRESULT_FROM_WIN32(GetLastError()),
                        "Load Microsoft.Internal.FrameworkUdk.dll");
    }
    using GetWindowIdFunction = HRESULT(WINAPI*)(HWND, winrt::Microsoft::UI::WindowId*);
    const auto getWindowId = reinterpret_cast<GetWindowIdFunction>(
        GetProcAddress(frameworkUdk, "Windowing_GetWindowIdFromWindow"));
    if (!getWindowId) {
        widget::CheckHr(HRESULT_FROM_WIN32(GetLastError()), "Resolve Windowing_GetWindowIdFromWindow");
    }
    winrt::Microsoft::UI::WindowId id{};
    widget::CheckHr(getWindowId(window, &id), "Windowing_GetWindowIdFromWindow");
    return id;
}

} // namespace

namespace widget {

AcrylicHost::~AcrylicHost() {
    Reset();
}

bool AcrylicHost::Initialize(HWND window) {
    Reset();

    DispatcherQueueOptions options{
        sizeof(DispatcherQueueOptions),
        DQTYPE_THREAD_CURRENT,
        DQTAT_COM_STA,
    };
    CheckHr(CreateDispatcherQueueController(
                options,
                reinterpret_cast<ABI::Windows::System::IDispatcherQueueController**>(
                    dispatcherController_.ReleaseAndGetAddressOf())),
            "CreateDispatcherQueueController");

    BOOL enabled = TRUE;
    constexpr auto useHostBackdropBrush = static_cast<DWMWINDOWATTRIBUTE>(17);
    const HRESULT backdropAttribute =
        DwmSetWindowAttribute(window, useHostBackdropBrush, &enabled, sizeof(enabled));
    if (FAILED(backdropAttribute)) {
        return false;
    }

    compositor_ = winrt::Windows::UI::Composition::Compositor();
    auto interop = compositor_.as<ABI::Windows::UI::Composition::Desktop::ICompositorDesktopInterop>();
    CheckHr(interop->CreateDesktopWindowTarget(
                window, false,
                reinterpret_cast<ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget**>(
                    winrt::put_abi(target_))),
            "CreateDesktopWindowTarget");

    configuration_ = winrt::Microsoft::UI::Composition::SystemBackdrops::SystemBackdropConfiguration();
    configuration_.IsInputActive(true);
    configuration_.Theme(
        winrt::Microsoft::UI::Composition::SystemBackdrops::SystemBackdropTheme::Default);

    controller_ = winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicController();
    controller_.Kind(
        winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicKind::Thin);
    controller_.SetSystemBackdropConfiguration(configuration_);

    const auto windowId = WindowIdFromWindow(window);
    available_ = controller_.SetTarget(windowId, target_);
    return available_;
}

void AcrylicHost::SetInputActive(bool active) {
    if (configuration_) {
        configuration_.IsInputActive(active);
    }
}

void AcrylicHost::Reset() noexcept {
    available_ = false;
    if (controller_) {
        controller_.Close();
    }
    controller_ = nullptr;
    configuration_ = nullptr;
    target_ = nullptr;
    compositor_ = nullptr;
    dispatcherController_.Reset();
}

} // namespace widget
