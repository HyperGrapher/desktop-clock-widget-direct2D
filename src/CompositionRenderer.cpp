#include "CompositionRenderer.h"

#include "ClockMath.h"
#include "Win32Helpers.h"

#include <d2d1_2helper.h>

#include <cmath>
#include <numbers>
#include <string>

namespace widget {
namespace {

constexpr float kLogicalSize = 280.0f;
constexpr float kCenter = kLogicalSize / 2.0f;

D2D1_COLOR_F Color(std::uint32_t rgb, float alpha = 1.0f) {
    return D2D1::ColorF(rgb, alpha);
}

} // namespace

void CompositionRenderer::Initialize(HWND window, UINT dpi, bool unlocked,
                                     bool acrylicAvailable) {
    Reset();
    window_ = window;
    dpi_ = dpi;
    pixelSize_ = static_cast<UINT>(DipToPixels(kLogicalSize, dpi_));
    unlocked_ = unlocked;
    acrylicAvailable_ = acrylicAvailable;
    CreateDevices();
    CreateCompositionTree();
    DrawAll(unlocked_, acrylicAvailable_);
    ResynchronizeHands();
}

void CompositionRenderer::Resize(UINT dpi, bool unlocked, bool acrylicAvailable) {
    Initialize(window_, dpi, unlocked, acrylicAvailable);
}

void CompositionRenderer::Reset() noexcept {
    for (auto& layer : layers_) {
        layer = {};
    }
    rootVisual_.Reset();
    compositionTarget_.Reset();
    compositionDevice_.Reset();
    dwriteFactory_.Reset();
    d2dContext_.Reset();
    d2dDevice_.Reset();
    d2dFactory_.Reset();
    dxgiFactory_.Reset();
    dxgiDevice_.Reset();
    d3dDevice_.Reset();
}

void CompositionRenderer::CreateDevices() {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL level{};
    HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
                                       D3D11_SDK_VERSION, &d3dDevice_, &level, nullptr);
#if defined(_DEBUG)
    if (result == DXGI_ERROR_SDK_COMPONENT_MISSING) {
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
                                   D3D11_SDK_VERSION, &d3dDevice_, &level, nullptr);
    }
#endif
    CheckHr(result, "D3D11CreateDevice");
    CheckHr(d3dDevice_.As(&dxgiDevice_), "Query IDXGIDevice");

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    CheckHr(dxgiDevice_->GetAdapter(&adapter), "IDXGIDevice::GetAdapter");
    CheckHr(adapter->GetParent(IID_PPV_ARGS(&dxgiFactory_)), "Get DXGI factory");

    D2D1_FACTORY_OPTIONS options{};
#if defined(_DEBUG)
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    CheckHr(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1),
                              &options,
                              reinterpret_cast<void**>(d2dFactory_.ReleaseAndGetAddressOf())),
            "D2D1CreateFactory");
    CheckHr(d2dFactory_->CreateDevice(dxgiDevice_.Get(), &d2dDevice_),
            "ID2D1Factory1::CreateDevice");
    CheckHr(d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dContext_),
            "ID2D1Device::CreateDeviceContext");
    d2dContext_->SetUnitMode(D2D1_UNIT_MODE_DIPS);
    d2dContext_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    d2dContext_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    CheckHr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf())),
            "DWriteCreateFactory");
    CheckHr(DCompositionCreateDevice(dxgiDevice_.Get(), __uuidof(IDCompositionDevice),
                                     reinterpret_cast<void**>(compositionDevice_.GetAddressOf())),
            "DCompositionCreateDevice");
}

void CompositionRenderer::CreateCompositionTree() {
    CheckHr(compositionDevice_->CreateTargetForHwnd(window_, TRUE, &compositionTarget_),
            "CreateTargetForHwnd");
    CheckHr(compositionDevice_->CreateVisual(&rootVisual_), "Create root visual");

    CreateLayer(Get(LayerId::Face), false);
    CreateLayer(Get(LayerId::Hour), true);
    CreateLayer(Get(LayerId::Minute), true);
    CreateLayer(Get(LayerId::Second), true);
    CreateLayer(Get(LayerId::Cap), false);
    CreateLayer(Get(LayerId::Unlock), false);

    IDCompositionVisual* previous = nullptr;
    for (auto& layer : layers_) {
        CheckHr(rootVisual_->AddVisual(layer.visual.Get(), previous != nullptr, previous),
                "IDCompositionVisual::AddVisual");
        previous = layer.visual.Get();
    }
    CheckHr(compositionTarget_->SetRoot(rootVisual_.Get()), "IDCompositionTarget::SetRoot");
    CheckHr(compositionDevice_->Commit(), "IDCompositionDevice::Commit");
}

void CompositionRenderer::CreateLayer(Layer& layer, bool rotates) {
    DXGI_SWAP_CHAIN_DESC1 description{};
    description.Width = pixelSize_;
    description.Height = pixelSize_;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.Stereo = FALSE;
    description.SampleDesc.Count = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 2;
    description.Scaling = DXGI_SCALING_STRETCH;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    CheckHr(dxgiFactory_->CreateSwapChainForComposition(d3dDevice_.Get(), &description, nullptr,
                                                         &layer.swapChain),
            "CreateSwapChainForComposition");
    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    CheckHr(layer.swapChain->GetBuffer(0, IID_PPV_ARGS(&surface)), "IDXGISwapChain::GetBuffer");

    const auto properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        static_cast<float>(dpi_), static_cast<float>(dpi_));
    CheckHr(d2dContext_->CreateBitmapFromDxgiSurface(surface.Get(), &properties, &layer.bitmap),
            "CreateBitmapFromDxgiSurface");

    CheckHr(compositionDevice_->CreateVisual(&layer.visual), "Create layer visual");
    CheckHr(layer.visual->SetContent(layer.swapChain.Get()), "Set visual content");
    if (rotates) {
        CheckHr(compositionDevice_->CreateRotateTransform(&layer.rotation), "CreateRotateTransform");
        CheckHr(layer.rotation->SetCenterX(static_cast<float>(DipToPixels(kCenter, dpi_))),
                "SetCenterX");
        CheckHr(layer.rotation->SetCenterY(static_cast<float>(DipToPixels(kCenter, dpi_))),
                "SetCenterY");
        CheckHr(layer.visual->SetTransform(layer.rotation.Get()), "Set hand transform");
    }
}

void CompositionRenderer::DrawAll(bool unlocked, bool acrylicAvailable) {
    DrawFace(Get(LayerId::Face), acrylicAvailable);
    DrawHand(Get(LayerId::Hour), 70.0f, 9.0f, 7.0f, Color(0xF2F5FA, 0.96f));
    DrawHand(Get(LayerId::Minute), 98.0f, 11.0f, 5.0f, Color(0xF2F5FA, 0.96f));
    DrawHand(Get(LayerId::Second), 112.0f, 24.0f, 2.0f, Color(0xFF5964, 1.0f));
    DrawCap(Get(LayerId::Cap));
    DrawUnlockOutline(Get(LayerId::Unlock), unlocked);
}

void CompositionRenderer::BeginLayer(Layer& layer) {
    d2dContext_->SetTarget(layer.bitmap.Get());
    d2dContext_->BeginDraw();
    d2dContext_->Clear(D2D1::ColorF(0, 0.0f));
}

void CompositionRenderer::EndLayer(Layer& layer) {
    CheckHr(d2dContext_->EndDraw(), "ID2D1DeviceContext::EndDraw");
    CheckHr(layer.swapChain->Present(1, 0), "IDXGISwapChain::Present");
}

void CompositionRenderer::DrawFace(Layer& layer, bool acrylicAvailable) {
    BeginLayer(layer);

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    CheckHr(d2dContext_->CreateSolidColorBrush(
                acrylicAvailable ? Color(0x102238, 0.22f) : Color(0x142536, 0.82f), &brush),
            "Create face brush");
    const auto face = D2D1::Ellipse(D2D1::Point2F(kCenter, kCenter), 136.0f, 136.0f);
    d2dContext_->FillEllipse(face, brush.Get());

    brush->SetColor(Color(0xFFFFFF, 0.20f));
    d2dContext_->DrawEllipse(face, brush.Get(), 2.0f);
    const auto inner = D2D1::Ellipse(D2D1::Point2F(kCenter, kCenter), 131.0f, 131.0f);
    brush->SetColor(Color(0x07101A, 0.38f));
    d2dContext_->DrawEllipse(inner, brush.Get(), 3.0f);

    for (int index = 0; index < 60; ++index) {
        const double angle = (static_cast<double>(index) * 6.0 - 90.0) *
                             std::numbers::pi / 180.0;
        const bool major = index % 5 == 0;
        const float outerRadius = 121.0f;
        const float innerRadius = major ? 108.0f : 115.0f;
        const auto start = D2D1::Point2F(
            kCenter + innerRadius * static_cast<float>(std::cos(angle)),
            kCenter + innerRadius * static_cast<float>(std::sin(angle)));
        const auto end = D2D1::Point2F(
            kCenter + outerRadius * static_cast<float>(std::cos(angle)),
            kCenter + outerRadius * static_cast<float>(std::sin(angle)));
        brush->SetColor(major ? Color(0xFFFFFF, 0.90f) : Color(0xFFFFFF, 0.48f));
        d2dContext_->DrawLine(start, end, brush.Get(), major ? 3.0f : 1.2f);
    }

    Microsoft::WRL::ComPtr<IDWriteTextFormat> numberFormat;
    CheckHr(dwriteFactory_->CreateTextFormat(L"Segoe UI Semibold", nullptr,
                                             DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                             DWRITE_FONT_STYLE_NORMAL,
                                             DWRITE_FONT_STRETCH_NORMAL, 17.0f, L"",
                                             &numberFormat),
            "Create number format");
    numberFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    numberFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    brush->SetColor(Color(0xFFFFFF, 0.88f));
    for (int number = 1; number <= 12; ++number) {
        const double angle = (static_cast<double>(number) * 30.0 - 90.0) *
                             std::numbers::pi / 180.0;
        const float radius = 91.0f;
        const float x = kCenter + radius * static_cast<float>(std::cos(angle));
        const float y = kCenter + radius * static_cast<float>(std::sin(angle));
        const std::wstring text = std::to_wstring(number);
        d2dContext_->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), numberFormat.Get(),
                               D2D1::RectF(x - 16.0f, y - 13.0f, x + 16.0f, y + 13.0f),
                               brush.Get());
    }

    brush->SetColor(Color(0x07101A, 0.48f));
    const auto dateRect = D2D1::RoundedRect(D2D1::RectF(80.0f, 177.0f, 200.0f, 204.0f), 7.0f, 7.0f);
    d2dContext_->FillRoundedRectangle(dateRect, brush.Get());
    brush->SetColor(Color(0xFFFFFF, 0.15f));
    d2dContext_->DrawRoundedRectangle(dateRect, brush.Get(), 1.0f);

    wchar_t dateBuffer[96]{};
    if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, nullptr, nullptr, dateBuffer,
                        static_cast<int>(std::size(dateBuffer)), nullptr) == 0) {
        GetDateFormatEx(LOCALE_NAME_INVARIANT, 0, nullptr, L"yyyy-MM-dd", dateBuffer,
                        static_cast<int>(std::size(dateBuffer)), nullptr);
    }
    Microsoft::WRL::ComPtr<IDWriteTextFormat> dateFormat;
    CheckHr(dwriteFactory_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                             DWRITE_FONT_STYLE_NORMAL,
                                             DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"",
                                             &dateFormat),
            "Create date format");
    dateFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    dateFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    brush->SetColor(Color(0xFFFFFF, 0.90f));
    d2dContext_->DrawTextW(dateBuffer, static_cast<UINT32>(wcslen(dateBuffer)), dateFormat.Get(),
                           dateRect.rect, brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

    EndLayer(layer);
}

void CompositionRenderer::DrawHand(Layer& layer, float length, float tail, float width,
                                   D2D1_COLOR_F color) {
    BeginLayer(layer);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    CheckHr(d2dContext_->CreateSolidColorBrush(color, &brush), "Create hand brush");
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> stroke;
    D2D1_STROKE_STYLE_PROPERTIES properties = D2D1::StrokeStyleProperties();
    properties.startCap = D2D1_CAP_STYLE_ROUND;
    properties.endCap = D2D1_CAP_STYLE_ROUND;
    CheckHr(d2dFactory_->CreateStrokeStyle(properties, nullptr, 0, &stroke),
            "Create hand stroke style");
    d2dContext_->DrawLine(D2D1::Point2F(kCenter, kCenter + tail),
                           D2D1::Point2F(kCenter, kCenter - length), brush.Get(), width,
                           stroke.Get());
    EndLayer(layer);
}

void CompositionRenderer::DrawCap(Layer& layer) {
    BeginLayer(layer);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    CheckHr(d2dContext_->CreateSolidColorBrush(Color(0xFF5964), &brush), "Create cap brush");
    d2dContext_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kCenter, kCenter), 7.0f, 7.0f),
                             brush.Get());
    brush->SetColor(Color(0xFFFFFF, 0.75f));
    d2dContext_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kCenter - 2.0f, kCenter - 2.0f),
                                          1.6f, 1.6f), brush.Get());
    EndLayer(layer);
}

void CompositionRenderer::DrawUnlockOutline(Layer& layer, bool unlocked) {
    BeginLayer(layer);
    if (unlocked) {
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
        CheckHr(d2dContext_->CreateSolidColorBrush(Color(0x64B5FF, 0.95f), &brush),
                "Create unlock brush");
        float dashes[] = {3.0f, 3.0f};
        D2D1_STROKE_STYLE_PROPERTIES properties = D2D1::StrokeStyleProperties();
        properties.dashStyle = D2D1_DASH_STYLE_CUSTOM;
        Microsoft::WRL::ComPtr<ID2D1StrokeStyle> stroke;
        CheckHr(d2dFactory_->CreateStrokeStyle(properties, dashes, 2, &stroke),
                "Create unlock stroke");
        d2dContext_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(kCenter, kCenter), 136.5f, 136.5f),
                                 brush.Get(), 2.0f, stroke.Get());
    }
    EndLayer(layer);
}

void CompositionRenderer::SetUnlocked(bool unlocked) {
    unlocked_ = unlocked;
    DrawUnlockOutline(Get(LayerId::Unlock), unlocked_);
}

void CompositionRenderer::RefreshDate() {
    DrawFace(Get(LayerId::Face), acrylicAvailable_);
}

void CompositionRenderer::ApplyHandAnimation(Layer& layer, double startingAngle,
                                             double periodSeconds) {
    Microsoft::WRL::ComPtr<IDCompositionAnimation> animation;
    CheckHr(compositionDevice_->CreateAnimation(&animation), "Create hand animation");
    CheckHr(animation->AddCubic(0.0, static_cast<float>(startingAngle),
                                static_cast<float>(360.0 / periodSeconds), 0.0f, 0.0f),
            "AddCubic");
    CheckHr(animation->AddRepeat(periodSeconds, periodSeconds), "AddRepeat");
    CheckHr(layer.rotation->SetAngle(animation.Get()), "Set animated angle");
}

void CompositionRenderer::ResynchronizeHands() {
    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);
    const ClockAngles angles = ComputeClockAngles(localTime);
    ApplyHandAnimation(Get(LayerId::Hour), angles.hour, 12.0 * 60.0 * 60.0);
    ApplyHandAnimation(Get(LayerId::Minute), angles.minute, 60.0 * 60.0);
    ApplyHandAnimation(Get(LayerId::Second), angles.second, 60.0);
    CheckHr(compositionDevice_->Commit(), "Commit hand animations");
}

CompositionRenderer::Layer& CompositionRenderer::Get(LayerId id) noexcept {
    return layers_[static_cast<std::size_t>(id)];
}

} // namespace widget
