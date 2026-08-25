#pragma once

#include <windows.h>

#include <d2d1_2.h>
#include <d3d11_2.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

#include <array>

namespace widget {

class CompositionRenderer {
public:
    CompositionRenderer() = default;
    ~CompositionRenderer() = default;

    CompositionRenderer(const CompositionRenderer&) = delete;
    CompositionRenderer& operator=(const CompositionRenderer&) = delete;

    void Initialize(HWND window, UINT dpi, bool unlocked, bool acrylicAvailable);
    void Resize(UINT dpi, bool unlocked, bool acrylicAvailable);
    void SetUnlocked(bool unlocked);
    void RefreshDate();
    void ResynchronizeHands();
    void Reset() noexcept;

private:
    struct Layer {
        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
        Microsoft::WRL::ComPtr<IDCompositionVisual> visual;
        Microsoft::WRL::ComPtr<IDCompositionRotateTransform> rotation;
    };

    enum class LayerId : std::size_t { Face, Hour, Minute, Second, Cap, Unlock, Count };

    void CreateDevices();
    void CreateCompositionTree();
    void CreateLayer(Layer& layer, bool rotates);
    void DrawAll(bool unlocked, bool acrylicAvailable);
    void DrawFace(Layer& layer, bool acrylicAvailable);
    void DrawHand(Layer& layer, float length, float tail, float width, D2D1_COLOR_F color);
    void DrawCap(Layer& layer);
    void DrawUnlockOutline(Layer& layer, bool unlocked);
    void BeginLayer(Layer& layer);
    void EndLayer(Layer& layer);
    void ApplyHandAnimation(Layer& layer, double startingAngle, double periodSeconds);
    [[nodiscard]] Layer& Get(LayerId id) noexcept;

    HWND window_ = nullptr;
    UINT dpi_ = 96;
    UINT pixelSize_ = 280;
    bool unlocked_ = false;
    bool acrylicAvailable_ = false;

    Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice_;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice_;
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory_;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory_;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dContext_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<IDCompositionDevice> compositionDevice_;
    Microsoft::WRL::ComPtr<IDCompositionTarget> compositionTarget_;
    Microsoft::WRL::ComPtr<IDCompositionVisual> rootVisual_;
    std::array<Layer, static_cast<std::size_t>(LayerId::Count)> layers_{};
};

} // namespace widget
