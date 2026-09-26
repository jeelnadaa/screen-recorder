#include "overlay/RecordingHud.h"
#include <iostream>

#if defined(_WIN32)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#endif

namespace Recorder::Overlay {

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

                // Draw sleek rounded pill
                HBRUSH bgBrush = CreateSolidBrush(RGB(20, 20, 20));
                FillRect(hdc, &r, bgBrush);
                DeleteObject(bgBrush);

                // Draw red dot (or yellow if paused)
                COLORREF dotColor = (self && self->m_isPaused) ? RGB(255, 200, 0) : RGB(235, 40, 40);
                HBRUSH dotBrush = CreateSolidBrush(dotColor);
                HGDIOBJ oldBrush = SelectObject(hdc, dotBrush);
                HPEN nullPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
                HGDIOBJ oldPen = SelectObject(hdc, nullPen);

                Ellipse(hdc, 12, 11, 24, 23);

                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(dotBrush);
                DeleteObject(nullPen);

                // Format time string
                uint64_t totalSeconds = (self ? self->m_elapsedMs : 0) / 1000;
                uint64_t hrs = totalSeconds / 3600;
                uint64_t mins = (totalSeconds % 3600) / 60;
                uint64_t secs = totalSeconds % 60;

                wchar_t timeStr[32];
                swprintf_s(timeStr, L"%02llu:%02llu:%02llu", hrs, mins, secs);

                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(255, 255, 255));
                HFONT font = CreateFontW(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
                HGDIOBJ oldFont = SelectObject(hdc, font);

                RECT textRect = { 32, 7, r.right - 8, r.bottom };
                DrawTextW(hdc, timeStr, -1, &textRect, DT_SINGLELINE | DT_VCENTER);

                SelectObject(hdc, oldFont);
                DeleteObject(font);

                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_NCHITTEST:
                return HTCAPTION; // Allow dragging HUD around screen
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

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName,
            L"ScreenRecorderHUD",
            WS_POPUP | WS_VISIBLE,
            40, 40, 115, 34,
            nullptr, nullptr, wc.hInstance, nullptr
        );

        if (!m_hwnd) return false;

        SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        // Critical: Exclude this overlay window from screen capture!
        SetWindowDisplayAffinity(m_hwnd, WDA_EXCLUDEFROMCAPTURE);

        // Make edges rounded
        HRGN rgn = CreateRoundRectRgn(0, 0, 115, 34, 16, 16);
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

} // namespace Recorder::Overlay
