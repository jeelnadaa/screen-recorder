#include "../../include/ui/Direct2DRenderer.h"
#include <algorithm>
#include <cmath>

#if defined(_WIN32)
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#endif

namespace Recorder {
namespace Ui {

    Direct2DRenderer::Direct2DRenderer() = default;

    Direct2DRenderer::~Direct2DRenderer() {
#if defined(_WIN32)
        Cleanup();
#endif
    }

#if defined(_WIN32)
    void Direct2DRenderer::Cleanup() {
        if (m_captionFormat) { m_captionFormat->Release(); m_captionFormat = nullptr; }
        if (m_normalFormat) { m_normalFormat->Release(); m_normalFormat = nullptr; }
        if (m_boldFormat) { m_boldFormat->Release(); m_boldFormat = nullptr; }
        if (m_headerFormat) { m_headerFormat->Release(); m_headerFormat = nullptr; }
        if (m_largeFormat) { m_largeFormat->Release(); m_largeFormat = nullptr; }
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

        UINT winDpi = GetDpiForWindow(hwnd);
        m_dpi = (winDpi != 0) ? static_cast<float>(winDpi) : 96.0f;

        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

        D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            m_dpi, m_dpi
        );

        hr = m_d2dFactory->CreateHwndRenderTarget(
            rtProps,
            D2D1::HwndRenderTargetProperties(hwnd, size),
            &m_renderTarget
        );
        if (FAILED(hr)) return false;

        hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_solidBrush);
        if (FAILED(hr)) return false;

        // Fonts matching Windows Fluent Design System (Segoe UI)
        m_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"en-us", &m_captionFormat);
        m_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &m_normalFormat);
        m_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &m_boldFormat);
        m_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 17.0f, L"en-us", &m_headerFormat);
        m_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &m_largeFormat);

        m_initialized = true;
        return true;
    }

    void Direct2DRenderer::SetDpi(float dpi) {
        m_dpi = dpi;
        if (m_renderTarget) {
            m_renderTarget->SetDpi(m_dpi, m_dpi);
        }
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

    void Direct2DRenderer::DrawCircle(float cx, float cy, float radius, const ColorRGB& color, bool fill, float strokeWidth) {
        if (!m_renderTarget || !m_solidBrush) return;

        m_solidBrush->SetColor(D2D1::ColorF(color.r, color.g, color.b, color.a));
        D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius);

        if (fill) {
            m_renderTarget->FillEllipse(&ellipse, m_solidBrush);
        } else {
            m_renderTarget->DrawEllipse(&ellipse, m_solidBrush, strokeWidth);
        }
    }

    void Direct2DRenderer::DrawLine(float x1, float y1, float x2, float y2, const ColorRGB& color, float strokeWidth) {
        if (!m_renderTarget || !m_solidBrush) return;

        m_solidBrush->SetColor(D2D1::ColorF(color.r, color.g, color.b, color.a));
        m_renderTarget->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), m_solidBrush, strokeWidth);
    }

    void Direct2DRenderer::DrawText(const std::wstring& text, float left, float top, float right, float bottom,
                                   const ColorRGB& color, float fontSize, bool bold, bool center, bool vcenter) {
        if (!m_renderTarget || !m_solidBrush || text.empty()) return;

        m_solidBrush->SetColor(D2D1::ColorF(color.r, color.g, color.b, color.a));

        IDWriteTextFormat* format = m_normalFormat;
        if (fontSize <= 11.5f) {
            format = m_captionFormat;
        } else if (fontSize >= 19.0f) {
            format = m_largeFormat;
        } else if (fontSize >= 16.0f) {
            format = m_headerFormat;
        } else if (bold) {
            format = m_boldFormat;
        }

        if (center) {
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        } else {
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        if (vcenter) {
            format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        } else {
            format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        }

        D2D1_RECT_F rect = D2D1::RectF(left, top, right, bottom);
        m_renderTarget->DrawTextW(text.data(), static_cast<UINT32>(text.size()), format, &rect, m_solidBrush);
    }

    void Direct2DRenderer::DrawIcon(IconType type, float x, float y, float size, const ColorRGB& color) {
        if (!m_renderTarget || !m_solidBrush) return;

        m_solidBrush->SetColor(D2D1::ColorF(color.r, color.g, color.b, color.a));
        float stroke = 1.5f;

        switch (type) {
            case IconType::Source: {
                // Crop / crosshair source icon
                float pad = size * 0.15f;
                DrawRect(x + pad, y + pad, x + size - pad, y + size - pad, color, stroke, 2.0f);
                DrawLine(x, y + size * 0.5f, x + pad * 1.5f, y + size * 0.5f, color, stroke);
                DrawLine(x + size - pad * 1.5f, y + size * 0.5f, x + size, y + size * 0.5f, color, stroke);
                DrawLine(x + size * 0.5f, y, x + size * 0.5f, y + pad * 1.5f, color, stroke);
                DrawLine(x + size * 0.5f, y + size - pad * 1.5f, x + size * 0.5f, y + size, color, stroke);
                break;
            }
            case IconType::Video: {
                // Camera body + trapezoid lens
                float bw = size * 0.65f;
                float bh = size * 0.55f;
                float by = y + (size - bh) * 0.5f;
                DrawRect(x, by, x + bw, by + bh, color, stroke, 3.0f);
                // Lens triangle
                DrawLine(x + bw, y + size * 0.35f, x + size, y + size * 0.22f, color, stroke);
                DrawLine(x + size, y + size * 0.22f, x + size, y + size * 0.78f, color, stroke);
                DrawLine(x + size, y + size * 0.78f, x + bw, y + size * 0.65f, color, stroke);
                break;
            }
            case IconType::Audio: {
                // Microphone: capsule + arc + stand
                float mw = size * 0.32f;
                float mh = size * 0.52f;
                float mx = x + (size - mw) * 0.5f;
                float my = y + size * 0.12f;
                FillRect(mx, my, mx + mw, my + mh, color, mw * 0.5f);
                // Cup arc
                DrawLine(mx - 3.0f, my + mh * 0.5f, mx - 3.0f, my + mh + 2.0f, color, stroke);
                DrawLine(mx - 3.0f, my + mh + 2.0f, mx + mw + 3.0f, my + mh + 2.0f, color, stroke);
                DrawLine(mx + mw + 3.0f, my + mh + 2.0f, mx + mw + 3.0f, my + mh * 0.5f, color, stroke);
                // Stand & Base
                float cx = x + size * 0.5f;
                DrawLine(cx, my + mh + 2.0f, cx, y + size - 2.0f, color, stroke);
                DrawLine(cx - 5.0f, y + size - 2.0f, cx + 5.0f, y + size - 2.0f, color, stroke);
                break;
            }
            case IconType::Hotkeys: {
                // Keyboard: outline + key dots
                float padY = size * 0.22f;
                DrawRect(x, y + padY, x + size, y + size - padY, color, stroke, 3.0f);
                DrawLine(x + size * 0.25f, y + size * 0.42f, x + size * 0.38f, y + size * 0.42f, color, stroke);
                DrawLine(x + size * 0.48f, y + size * 0.42f, x + size * 0.60f, y + size * 0.42f, color, stroke);
                DrawLine(x + size * 0.70f, y + size * 0.42f, x + size * 0.82f, y + size * 0.42f, color, stroke);
                DrawLine(x + size * 0.35f, y + size * 0.62f, x + size * 0.65f, y + size * 0.62f, color, stroke);
                break;
            }
            case IconType::Output: {
                // Folder icon
                float fy = y + size * 0.2f;
                float fh = size * 0.6f;
                DrawLine(x, fy, x + size * 0.4f, fy, color, stroke);
                DrawLine(x + size * 0.4f, fy, x + size * 0.5f, fy + 4.0f, color, stroke);
                DrawLine(x + size * 0.5f, fy + 4.0f, x + size, fy + 4.0f, color, stroke);
                DrawLine(x + size, fy + 4.0f, x + size, fy + fh, color, stroke);
                DrawLine(x + size, fy + fh, x, fy + fh, color, stroke);
                DrawLine(x, fy + fh, x, fy, color, stroke);
                break;
            }
            case IconType::Library: {
                // Clock with counter-clockwise history circle
                float cx = x + size * 0.5f;
                float cy = y + size * 0.5f;
                float r = size * 0.42f;
                DrawCircle(cx, cy, r, color, false, stroke);
                DrawLine(cx, cy, cx, cy - r * 0.6f, color, stroke);
                DrawLine(cx, cy, cx + r * 0.5f, cy, color, stroke);
                break;
            }
            case IconType::Advanced: {
                // Equalizer / Sliders
                float s1 = x + size * 0.25f;
                float s2 = x + size * 0.50f;
                float s3 = x + size * 0.75f;
                DrawLine(s1, y + 2.0f, s1, y + size - 2.0f, color, stroke);
                DrawLine(s2, y + 2.0f, s2, y + size - 2.0f, color, stroke);
                DrawLine(s3, y + 2.0f, s3, y + size - 2.0f, color, stroke);
                FillRect(s1 - 3.0f, y + size * 0.35f, s1 + 3.0f, y + size * 0.55f, color, 1.5f);
                FillRect(s2 - 3.0f, y + size * 0.60f, s2 + 3.0f, y + size * 0.80f, color, 1.5f);
                FillRect(s3 - 3.0f, y + size * 0.20f, s3 + 3.0f, y + size * 0.40f, color, 1.5f);
                break;
            }
            case IconType::Monitor: {
                // Display monitor
                float scrH = size * 0.62f;
                DrawRect(x, y, x + size, y + scrH, color, stroke, 3.0f);
                float cx = x + size * 0.5f;
                DrawLine(cx, y + scrH, cx, y + size - 1.0f, color, stroke);
                DrawLine(cx - size * 0.25f, y + size - 1.0f, cx + size * 0.25f, y + size - 1.0f, color, stroke);
                break;
            }
            case IconType::Window: {
                // Window frame
                DrawRect(x, y, x + size, y + size, color, stroke, 3.0f);
                DrawLine(x, y + size * 0.28f, x + size, y + size * 0.28f, color, stroke);
                break;
            }
            case IconType::App: {
                // 4 squares in 2x2 grid with +
                float gap = 3.0f;
                float qw = (size - gap) * 0.5f;
                DrawRect(x, y, x + qw, y + qw, color, stroke, 1.5f);
                DrawRect(x + qw + gap, y, x + size, y + qw, color, stroke, 1.5f);
                DrawRect(x, y + qw + gap, x + qw, y + size, color, stroke, 1.5f);
                // Top-right or bottom-right plus
                float px = x + qw + gap + qw * 0.5f;
                float py = y + qw + gap + qw * 0.5f;
                DrawLine(px - 3.0f, py, px + 3.0f, py, color, stroke);
                DrawLine(px, py - 3.0f, px, py + 3.0f, color, stroke);
                break;
            }
            case IconType::Region: {
                // Region crop marks
                float arm = size * 0.35f;
                // Top-Left
                DrawLine(x, y, x + arm, y, color, stroke);
                DrawLine(x, y, x, y + arm, color, stroke);
                // Top-Right
                DrawLine(x + size, y, x + size - arm, y, color, stroke);
                DrawLine(x + size, y, x + size, y + arm, color, stroke);
                // Bottom-Left
                DrawLine(x, y + size, x + arm, y + size, color, stroke);
                DrawLine(x, y + size, x, y + size - arm, color, stroke);
                // Bottom-Right
                DrawLine(x + size, y + size, x + size - arm, y + size, color, stroke);
                DrawLine(x + size, y + size, x + size, y + size - arm, color, stroke);
                break;
            }
            case IconType::ChevronDown: {
                // Down arrow v
                float cx = x + size * 0.5f;
                float cy = y + size * 0.55f;
                float w = size * 0.35f;
                float h = size * 0.22f;
                DrawLine(cx - w, cy - h, cx, cy + h, color, stroke);
                DrawLine(cx, cy + h, cx + w, cy - h, color, stroke);
                break;
            }
            case IconType::Chip: {
                // CPU chip with pins
                float pad = size * 0.22f;
                DrawRect(x + pad, y + pad, x + size - pad, y + size - pad, color, stroke, 2.0f);
                // Pins
                DrawLine(x, y + size * 0.38f, x + pad, y + size * 0.38f, color, stroke);
                DrawLine(x, y + size * 0.62f, x + pad, y + size * 0.62f, color, stroke);
                DrawLine(x + size - pad, y + size * 0.38f, x + size, y + size * 0.38f, color, stroke);
                DrawLine(x + size - pad, y + size * 0.62f, x + size, y + size * 0.62f, color, stroke);
                break;
            }
            case IconType::Warning: {
                // Warning triangle
                float cx = x + size * 0.5f;
                DrawLine(cx, y + 2.0f, x + size - 1.0f, y + size - 2.0f, color, stroke);
                DrawLine(x + size - 1.0f, y + size - 2.0f, x + 1.0f, y + size - 2.0f, color, stroke);
                DrawLine(x + 1.0f, y + size - 2.0f, cx, y + 2.0f, color, stroke);
                DrawLine(cx, y + size * 0.40f, cx, y + size * 0.65f, color, stroke);
                DrawCircle(cx, y + size * 0.78f, 1.0f, color, true);
                break;
            }
            case IconType::Record: {
                // Hollow ring
                float cx = x + size * 0.5f;
                float cy = y + size * 0.5f;
                DrawCircle(cx, cy, size * 0.42f, color, false, stroke + 0.5f);
                break;
            }
            case IconType::Camera: {
                // Photo camera
                float padY = size * 0.2f;
                DrawRect(x, y + padY, x + size, y + size, color, stroke, 3.0f);
                DrawRect(x + size * 0.3f, y + padY - 3.0f, x + size * 0.7f, y + padY, color, stroke, 1.0f);
                DrawCircle(x + size * 0.5f, y + padY + (size - padY) * 0.5f, size * 0.22f, color, false, stroke);
                break;
            }
            case IconType::DotsMenu: {
                // Horizontal 3 dots
                float cy = y + size * 0.5f;
                float r = 1.5f;
                DrawCircle(x + size * 0.2f, cy, r, color, true);
                DrawCircle(x + size * 0.5f, cy, r, color, true);
                DrawCircle(x + size * 0.8f, cy, r, color, true);
                break;
            }
            case IconType::Search: {
                // Search magnifying glass
                float cx = x + size * 0.42f;
                float cy = y + size * 0.42f;
                float r = size * 0.32f;
                DrawCircle(cx, cy, r, color, false, stroke);
                DrawLine(cx + r * 0.7f, cy + r * 0.7f, x + size - 1.0f, y + size - 1.0f, color, stroke + 0.5f);
                break;
            }
            case IconType::Folder: {
                float fy = y + size * 0.2f;
                float fh = size * 0.65f;
                DrawLine(x, fy, x + size * 0.4f, fy, color, stroke);
                DrawLine(x + size * 0.4f, fy, x + size * 0.52f, fy + 4.0f, color, stroke);
                DrawLine(x + size * 0.52f, fy + 4.0f, x + size, fy + 4.0f, color, stroke);
                DrawLine(x + size, fy + 4.0f, x + size, fy + fh, color, stroke);
                DrawLine(x + size, fy + fh, x, fy + fh, color, stroke);
                DrawLine(x, fy + fh, x, fy, color, stroke);
                break;
            }
            case IconType::Share: {
                // Connected nodes
                float n1x = x + size * 0.8f, n1y = y + size * 0.25f;
                float n2x = x + size * 0.25f, n2y = y + size * 0.5f;
                float n3x = x + size * 0.8f, n3y = y + size * 0.75f;
                DrawLine(n2x, n2y, n1x, n1y, color, stroke);
                DrawLine(n2x, n2y, n3x, n3y, color, stroke);
                DrawCircle(n1x, n1y, 2.5f, color, true);
                DrawCircle(n2x, n2y, 2.5f, color, true);
                DrawCircle(n3x, n3y, 2.5f, color, true);
                break;
            }
            case IconType::Trash: {
                // Trash bin
                float ty = y + size * 0.25f;
                DrawLine(x + size * 0.15f, ty, x + size * 0.85f, ty, color, stroke);
                DrawLine(x + size * 0.35f, ty - 2.5f, x + size * 0.65f, ty - 2.5f, color, stroke);
                DrawLine(x + size * 0.25f, ty, x + size * 0.30f, y + size - 2.0f, color, stroke);
                DrawLine(x + size * 0.75f, ty, x + size * 0.70f, y + size - 2.0f, color, stroke);
                DrawLine(x + size * 0.30f, y + size - 2.0f, x + size * 0.70f, y + size - 2.0f, color, stroke);
                break;
            }
        }
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

} // namespace Ui
} // namespace Recorder
