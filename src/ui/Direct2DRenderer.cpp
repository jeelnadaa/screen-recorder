#include "ui/Direct2DRenderer.h"
#include <algorithm>

#if defined(_WIN32)
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#endif

namespace Recorder::Ui {

    Direct2DRenderer::Direct2DRenderer() = default;

    Direct2DRenderer::~Direct2DRenderer() {
#if defined(_WIN32)
        Cleanup();
#endif
    }

#if defined(_WIN32)
    void Direct2DRenderer::Cleanup() {
        if (m_normalFormat) { m_normalFormat->Release(); m_normalFormat = nullptr; }
        if (m_boldFormat) { m_boldFormat->Release(); m_boldFormat = nullptr; }
        if (m_titleFormat) { m_titleFormat->Release(); m_titleFormat = nullptr; }
        if (m_solidBrush) { m_solidBrush->Release(); m_solidBrush = nullptr; }
        if (m_renderTarget) { m_renderTarget->Release(); m_renderTarget = nullptr; }
        if (m_dwriteFactory) { m_dwriteFactory->Release(); m_dwriteFactory = nullptr; }
        if (m_d2dFactory) { m_d2dFactory->Release(); m_d2dFactory = nullptr; }
        m_initialized = false;
    }

    bool Direct2DRenderer::Initialize(HWND hwnd) {
        Cleanup();

        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory);
        if (FAILED(hr)) return false;

        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_dwriteFactory));
        if (FAILED(hr)) return false;

        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

        hr = m_d2dFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd, size),
            &m_renderTarget
        );
        if (FAILED(hr)) return false;

        hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_solidBrush);
        if (FAILED(hr)) return false;

        // Fonts
        m_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &m_normalFormat);
        m_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &m_boldFormat);
        m_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &m_titleFormat);

        m_initialized = true;
        return true;
    }

    void Direct2DRenderer::Resize(uint32_t width, uint32_t height) {
        if (m_renderTarget) {
            m_renderTarget->Resize(D2D1::SizeU(width, height));
        }
    }

    void Direct2DRenderer::BeginDraw() {
        if (m_renderTarget) {
            m_renderTarget->BeginDraw();
        }
    }

    void Direct2DRenderer::EndDraw() {
        if (m_renderTarget) {
            m_renderTarget->EndDraw();
        }
    }

    void Direct2DRenderer::Clear(const ColorRGB& color) {
        if (m_renderTarget) {
            m_renderTarget->Clear(D2D1::ColorF(color.r, color.g, color.b, color.a));
        }
    }

    void Direct2DRenderer::FillRect(float left, float top, float right, float bottom, const ColorRGB& color, float cornerRadius) {
        if (!m_renderTarget || !m_solidBrush) return;

        m_solidBrush->SetColor(D2D1::ColorF(color.r, color.g, color.b, color.a));
        D2D1_RECT_F rect = D2D1::RectF(left, top, right, bottom);

        if (cornerRadius > 0.0f) {
            D2D1_ROUNDED_RECT rrect = D2D1::RoundedRect(rect, cornerRadius, cornerRadius);
            m_renderTarget->FillRoundedRectangle(&rrect, m_solidBrush);
        } else {
            m_renderTarget->FillRectangle(&rect, m_solidBrush);
        }
    }

    void Direct2DRenderer::DrawRect(float left, float top, float right, float bottom, const ColorRGB& color, float strokeWidth, float cornerRadius) {
        if (!m_renderTarget || !m_solidBrush) return;

        m_solidBrush->SetColor(D2D1::ColorF(color.r, color.g, color.b, color.a));
        D2D1_RECT_F rect = D2D1::RectF(left, top, right, bottom);

        if (cornerRadius > 0.0f) {
            D2D1_ROUNDED_RECT rrect = D2D1::RoundedRect(rect, cornerRadius, cornerRadius);
            m_renderTarget->DrawRoundedRectangle(&rrect, m_solidBrush, strokeWidth);
        } else {
            m_renderTarget->DrawRectangle(&rect, m_solidBrush, strokeWidth);
        }
    }

    void Direct2DRenderer::DrawText(const std::wstring& text, float left, float top, float right, float bottom,
                                   const ColorRGB& color, float /*fontSize*/, bool bold, bool center) {
        if (!m_renderTarget || !m_solidBrush || text.empty()) return;

        m_solidBrush->SetColor(D2D1::ColorF(color.r, color.g, color.b, color.a));
        IDWriteTextFormat* format = bold ? m_boldFormat : m_normalFormat;

        if (center) {
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        } else {
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        }

        D2D1_RECT_F rect = D2D1::RectF(left, top, right, bottom);
        m_renderTarget->DrawTextW(text.data(), static_cast<UINT32>(text.size()), format, &rect, m_solidBrush);
    }

    void Direct2DRenderer::DrawProgressBar(float left, float top, float right, float bottom, float progress, const ColorRGB& fillColor) {
        FillRect(left, top, right, bottom, Theme::SurfaceCard, 4.0f);
        float width = (right - left) * std::clamp(progress, 0.0f, 1.0f);
        if (width > 0.0f) {
            FillRect(left, top, left + width, bottom, fillColor, 4.0f);
        }
    }

    void Direct2DRenderer::DrawVuMeter(float left, float top, float right, float bottom, float level) {
        FillRect(left, top, right, bottom, Theme::SurfaceCard, 3.0f);
        float clamped = std::clamp(level, 0.0f, 1.0f);
        float width = (right - left) * clamped;

        ColorRGB barColor = (clamped > 0.85f) ? Theme::RecordRed : ((clamped > 0.65f) ? Theme::WarningYellow : Theme::SuccessGreen);
        if (width > 0.0f) {
            FillRect(left, top, left + width, bottom, barColor, 3.0f);
        }
    }
#endif

} // namespace Recorder::Ui
