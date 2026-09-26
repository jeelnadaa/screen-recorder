#include "../../include/overlay/RecordingHud.h"
#include <iostream>

#if defined(_WIN32)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#endif

namespace Recorder {
namespace Overlay {

    RecordingHud::RecordingHud() = default;

    RecordingHud::~RecordingHud() {
        Destroy();
    }

#if defined(_WIN32)
    LRESULT CALLBACK RecordingHud::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        auto* self = reinterpret_cast<RecordingHud*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);

                RECT r;
                GetClientRect(hwnd, &r);

                // Deep black pill background matching Image 2
                HBRUSH bgBrush = CreateSolidBrush(RGB(18, 18, 20));
                FillRect(hdc, &r, bgBrush);
                DeleteObject(bgBrush);

                // Antialiased red dot (or amber if paused)
                COLORREF dotColor = (self && self->m_isPaused) ? RGB(245, 181, 26) : RGB(234, 46, 46);
                HBRUSH dotBrush = CreateSolidBrush(dotColor);
                HGDIOBJ oldBrush = SelectObject(hdc, dotBrush);
                HPEN nullPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
                HGDIOBJ oldPen = SelectObject(hdc, nullPen);

                // 10px diameter dot, perfectly centered
                int dotY = (r.bottom - 10) / 2;
                Ellipse(hdc, 16, dotY, 26, dotY + 10);

                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(dotBrush);
                DeleteObject(nullPen);

                // Format time string: "02:14" matching Image 2
                uint64_t totalSeconds = (self ? self->m_elapsedMs : 0) / 1000;
                uint64_t hrs = totalSeconds / 3600;
                uint64_t mins = (totalSeconds % 3600) / 60;
                uint64_t secs = totalSeconds % 60;

                wchar_t timeStr[32];
                if (hrs > 0) {
                    swprintf_s(timeStr, L"%02llu:%02llu:%02llu", hrs, mins, secs);
                } else {
                    swprintf_s(timeStr, L"%02llu:%02llu", mins, secs);
                }

                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(245, 245, 247));
                HFONT font = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
                HGDIOBJ oldFont = SelectObject(hdc, font);

                RECT textRect = { 32, 0, r.right - 12, r.bottom };
                DrawTextW(hdc, timeStr, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

                SelectObject(hdc, oldFont);
                DeleteObject(font);

                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_NCHITTEST:
                return HTCAPTION; // Allow dragging HUD smoothly anywhere on screen
            default:
                return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }
#endif

    bool RecordingHud::Create() {
#if defined(_WIN32)
        if (m_hwnd) return true;

        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"ScreenRecorderHudClass";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int hudW = 105;
        int hudH = 36;
        int posX = screenW - hudW - 40;
        int posY = 40;

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName,
            L"ScreenRecorderHUD",
            WS_POPUP | WS_VISIBLE,
            posX, posY, hudW, hudH,
            nullptr, nullptr, wc.hInstance, nullptr
        );

        if (!m_hwnd) return false;

        SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        // Critical requirement: Exclude overlay from capture via Windows API
        SetWindowDisplayAffinity(m_hwnd, WDA_EXCLUDEFROMCAPTURE);

        // Rounded pill capsule region
        HRGN rgn = CreateRoundRectRgn(0, 0, hudW, hudH, hudH, hudH);
        SetWindowRgn(m_hwnd, rgn, TRUE);

        return true;
#else
        return true;
#endif
    }

    void RecordingHud::Destroy() {
#if defined(_WIN32)
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
#endif
    }

    void RecordingHud::Show() {
#if defined(_WIN32)
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
        }
#endif
    }

    void RecordingHud::Hide() {
#if defined(_WIN32)
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_HIDE);
        }
#endif
    }

    void RecordingHud::Update(uint64_t elapsedMs, bool isPaused) {
        m_elapsedMs = elapsedMs;
        m_isPaused = isPaused;
#if defined(_WIN32)
        if (m_hwnd) {
            InvalidateRect(m_hwnd, nullptr, TRUE);
            UpdateWindow(m_hwnd);
        }
#endif
    }

} // namespace Overlay
} // namespace Recorder
