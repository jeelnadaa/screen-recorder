#include "../../include/overlay/RecordingHud.h"
#include "../../include/core/Engine.h"
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

                // Deep obsidian backdrop matching user's dark fluent theme
                HBRUSH bgBrush = CreateSolidBrush(RGB(16, 17, 21));
                FillRect(hdc, &r, bgBrush);
                DeleteObject(bgBrush);

                bool isPaused = self ? self->m_isPaused : false;
                COLORREF primaryColor = isPaused ? RGB(245, 181, 26) : RGB(234, 46, 46);

                // Outer border glow
                HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(46, 48, 58));
                HGDIOBJ oldBorderPen = SelectObject(hdc, borderPen);
                HGDIOBJ oldBorderBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                RoundRect(hdc, 0, 0, r.right, r.bottom, 26, 26);
                SelectObject(hdc, oldBorderBrush);
                SelectObject(hdc, oldBorderPen);
                DeleteObject(borderPen);

                // Prominent Pulsating Record Dot (14px diameter) with soft outer glow halo (22px)
                HBRUSH haloBrush = CreateSolidBrush(isPaused ? RGB(70, 52, 10) : RGB(70, 18, 18));
                HGDIOBJ oldBrush = SelectObject(hdc, haloBrush);
                HPEN nullPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
                HGDIOBJ oldPen = SelectObject(hdc, nullPen);

                int haloY = (r.bottom - 22) / 2;
                Ellipse(hdc, 15, haloY, 37, haloY + 22);

                HBRUSH dotBrush = CreateSolidBrush(primaryColor);
                SelectObject(hdc, dotBrush);
                int dotY = (r.bottom - 14) / 2;
                Ellipse(hdc, 19, dotY, 33, dotY + 14);

                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(haloBrush);
                DeleteObject(dotBrush);
                DeleteObject(nullPen);

                // Format time string: "02:14" in bold Segoe UI
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
                HFONT font = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
                HGDIOBJ oldFont = SelectObject(hdc, font);

                RECT textRect = { 42, 0, r.right - 44, r.bottom };
                DrawTextW(hdc, timeStr, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

                // Mini Stop Button Icon on right side
                HBRUSH stopBrush = CreateSolidBrush(RGB(180, 180, 190));
                SelectObject(hdc, stopBrush);
                int stopY = (r.bottom - 12) / 2;
                RECT stopRect = { r.right - 30, stopY, r.right - 18, stopY + 12 };
                FillRect(hdc, &stopRect, stopBrush);
                DeleteObject(stopBrush);

                SelectObject(hdc, oldFont);
                DeleteObject(font);

                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_LBUTTONDOWN: {
                int x = LOWORD(lParam);
                RECT r;
                GetClientRect(hwnd, &r);
                // If user clicks the stop button area on the right, stop recording!
                if (x >= r.right - 38) {
                    Core::Engine::Instance().StopRecording();
                    return 0;
                }
                // Otherwise allow dragging
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
                return 0;
            }
            case WM_NCHITTEST:
                return HTCAPTION; // Dragging anywhere on pill
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
        // Prominent, enlarged floating indicator pill (180 x 52)
        int hudW = 180;
        int hudH = 52;
        int posX = screenW - hudW - 48;
        int posY = 48;

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            wc.lpszClassName,
            L"ScreenRecorderHUD",
            WS_POPUP | WS_VISIBLE,
            posX, posY, hudW, hudH,
            nullptr, nullptr, wc.hInstance, nullptr
        );

        if (!m_hwnd) return false;

        SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        // Critical requirement: Exclude overlay window from screen capture
        SetWindowDisplayAffinity(m_hwnd, WDA_EXCLUDEFROMCAPTURE);

        // Smooth rounded capsule shape (radius 26 = half of 52)
        HRGN rgn = CreateRoundRectRgn(0, 0, hudW + 1, hudH + 1, hudH, hudH);
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
