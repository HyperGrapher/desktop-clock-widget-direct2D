#pragma once

#include <windows.h>

#include <wrl/client.h>

#include <winrt/Microsoft.UI.Composition.SystemBackdrops.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Composition.Desktop.h>

namespace widget {

class AcrylicHost {
public:
    AcrylicHost() = default;
    ~AcrylicHost();

    AcrylicHost(const AcrylicHost&) = delete;
    AcrylicHost& operator=(const AcrylicHost&) = delete;

    bool Initialize(HWND window);
    void SetInputActive(bool active);
    void Reset() noexcept;

    [[nodiscard]] bool IsAvailable() const noexcept { return available_; }

private:
    Microsoft::WRL::ComPtr<IUnknown> dispatcherController_;
    winrt::Windows::UI::Composition::Compositor compositor_{nullptr};
    winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget target_{nullptr};
    winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicController controller_{nullptr};
    winrt::Microsoft::UI::Composition::SystemBackdrops::SystemBackdropConfiguration configuration_{nullptr};
    bool available_ = false;
};

} // namespace widget
