#pragma once

#include "UITheme.h"
#include <string>

#if defined(_WIN32)
#include <d2d1.h>
#include <dwrite.h>
#endif

namespace Recorder::Ui {

    class Direct2DRenderer {
    public:
        Direct2DRenderer();
        ~Direct2DRenderer();

#if defined(_WIN32)
        bool Initialize(HWND hwnd);
        void Resize(uint32_t width, uint32_t height);
        void Cleanup();

        void BeginDraw();
        void EndDraw();

        void Clear(const ColorRGB& color = Theme::BackgroundDark);
        void FillRect(float left, float top, float right, float bottom, const ColorRGB& color, float cornerRadius = 0.0f);
        void DrawRect(float left, float top, float right, float bottom, const ColorRGB& color, float strokeWidth = 1.0f, float cornerRadius = 0.0f);
        
        void DrawText(const std::wstring& text, float left, float top, float right, float bottom,
                      const ColorRGB& color, float fontSize = 14.0f, bool bold = false, bool center = false);

        void DrawProgressBar(float left, float top, float right, float bottom, float progress, const ColorRGB& fillColor);
        void DrawVuMeter(float left, float top, float right, float bottom, float level);

    private:
        ID2D1Factory* m_d2dFactory = nullptr;
        IDWriteFactory* m_dwriteFactory = nullptr;
        ID2D1HwndRenderTarget* m_renderTarget = nullptr;
        ID2D1SolidColorBrush* m_solidBrush = nullptr;
        IDWriteTextFormat* m_normalFormat = nullptr;
        IDWriteTextFormat* m_boldFormat = nullptr;
        IDWriteTextFormat* m_titleFormat = nullptr;
#endif

        bool m_initialized = false;
    };

} // namespace Recorder::Ui
