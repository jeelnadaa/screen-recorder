#pragma once

#include "UITheme.h"
#include <string>

#if defined(_WIN32)
#include <d2d1.h>
#include <dwrite.h>
#endif

namespace Recorder {
namespace Ui {

    class Direct2DRenderer {
    public:
        Direct2DRenderer();
        ~Direct2DRenderer();

#if defined(_WIN32)
        bool Initialize(HWND hwnd);
        void Resize(uint32_t width, uint32_t height);
        void SetDpi(float dpi);
        float GetDpi() const { return m_dpi; }
        void Cleanup();

        void BeginDraw();
        void EndDraw();

        void Clear(const ColorRGB& color = Theme::BackgroundDark);
        void FillRect(float left, float top, float right, float bottom, const ColorRGB& color, float cornerRadius = 0.0f);
        void DrawRect(float left, float top, float right, float bottom, const ColorRGB& color, float strokeWidth = 1.0f, float cornerRadius = 0.0f);
        void DrawCircle(float cx, float cy, float radius, const ColorRGB& color, bool fill = true, float strokeWidth = 1.0f);
        void DrawLine(float x1, float y1, float x2, float y2, const ColorRGB& color, float strokeWidth = 1.0f);

        void DrawText(const std::wstring& text, float left, float top, float right, float bottom,
                      const ColorRGB& color, float fontSize = 13.0f, bool bold = false, bool center = false, bool vcenter = false);

        void DrawIcon(IconType type, float x, float y, float size, const ColorRGB& color);

        void DrawAppLogo(float cx, float cy, float radius);
        void DrawToggleSwitch(float x, float y, bool checked, bool hovered = false);
        void DrawPillButton(float left, float top, float right, float bottom, const std::wstring& text, bool active, bool hovered = false);
        void DrawSlider(float left, float top, float right, float bottom, float value, const ColorRGB& barColor);

        void DrawProgressBar(float left, float top, float right, float bottom, float progress, const ColorRGB& fillColor);
        void DrawVuMeter(float left, float top, float right, float bottom, float level);

    private:
        ID2D1Factory* m_d2dFactory = nullptr;
        IDWriteFactory* m_dwriteFactory = nullptr;
        ID2D1HwndRenderTarget* m_renderTarget = nullptr;
        ID2D1SolidColorBrush* m_solidBrush = nullptr;

        IDWriteTextFormat* m_captionFormat = nullptr;   // 11px
        IDWriteTextFormat* m_normalFormat = nullptr;    // 13px
        IDWriteTextFormat* m_boldFormat = nullptr;      // 13px semi-bold
        IDWriteTextFormat* m_headerFormat = nullptr;    // 17px semi-bold
        IDWriteTextFormat* m_largeFormat = nullptr;     // 20px bold

        float m_dpi = 96.0f;
#endif

        bool m_initialized = false;
    };

} // namespace Ui
} // namespace Recorder
