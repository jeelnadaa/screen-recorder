#include "../../include/ui/TrayIcon.h"
#include "../../include/core/Engine.h"
#include <iostream>

#if defined(_WIN32)
#pragma comment(lib, "shell32.lib")
#endif

namespace Recorder {
namespace Ui {

    TrayIcon::TrayIcon() = default;

    TrayIcon::~TrayIcon() {
#if defined(_WIN32)
        Remove();
#endif
    }

#if defined(_WIN32)
    bool TrayIcon::Initialize(HWND hwnd, UINT callbackMessageId, HICON icon) {
        m_hwnd = hwnd;

        m_nid.cbSize = sizeof(NOTIFYICONDATAW);
        m_nid.hWnd = m_hwnd;
        m_nid.uID = 1001;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = callbackMessageId;
        m_nid.hIcon = icon ? icon : LoadIconW(nullptr, IDI_APPLICATION);
        wcscpy_s(m_nid.szTip, L"Screen Recorder");

        m_added = (Shell_NotifyIconW(NIM_ADD, &m_nid) == TRUE);
        return m_added;
    }

    void TrayIcon::Remove() {
        if (m_added) {
            Shell_NotifyIconW(NIM_DELETE, &m_nid);
            m_added = false;
        }
    }

    void TrayIcon::ShowNotification(const std::wstring& title, const std::wstring& message) {
        if (!m_added) return;

        m_nid.uFlags |= NIF_INFO;
        wcscpy_s(m_nid.szInfoTitle, title.c_str());
        wcscpy_s(m_nid.szInfo, message.c_str());
        m_nid.dwInfoFlags = NIIF_INFO;

        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    }

    void TrayIcon::ShowContextMenu(int x, int y) {
        HMENU hMenu = CreatePopupMenu();

        auto state = Core::Engine::Instance().GetState();
        bool isRecording = (state == Core::EngineState::Recording || state == Core::EngineState::Paused);

        // Menu items matching Image 3:
        // 1. Start/Stop recording
        if (isRecording) {
            AppendMenuW(hMenu, MF_STRING, 2002, L"Stop recording");
        } else {
            AppendMenuW(hMenu, MF_STRING, 2002, L"Start recording");
        }

        // 2. Save replay clip
        AppendMenuW(hMenu, MF_STRING, 2004, L"Save replay clip");

        // 3. Screenshot
        AppendMenuW(hMenu, MF_STRING, 2006, L"Screenshot");

        // 4. Presets submenu
        HMENU hPresetMenu = CreatePopupMenu();
        AppendMenuW(hPresetMenu, MF_STRING, 2101, L"High Quality (1440p60 HEVC)");
        AppendMenuW(hPresetMenu, MF_STRING, 2102, L"Balanced (1080p60)");
        AppendMenuW(hPresetMenu, MF_STRING, 2103, L"Esports (1080p120)");
        AppendMenuW(hPresetMenu, MF_STRING, 2104, L"Low Latency (720p60)");
        AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hPresetMenu), L"Presets");

        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

        // 5. Open settings (Show/restore window)
        AppendMenuW(hMenu, MF_STRING, 2001, L"Open settings");

        // 6. Exit
        AppendMenuW(hMenu, MF_STRING, 2005, L"Exit");

        SetForegroundWindow(m_hwnd);
        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, x, y, 0, m_hwnd, nullptr);
        DestroyMenu(hMenu);

        switch (cmd) {
            case 2001: if (m_onRestore) m_onRestore(); break;
            case 2002: {
                if (isRecording) {
                    if (m_onStopRecord) m_onStopRecord();
                } else {
                    if (m_onStartRecord) m_onStartRecord();
                }
                break;
            }
            case 2004: if (m_onSaveReplay) m_onSaveReplay(); break;
            case 2006: if (m_onScreenshot) m_onScreenshot(); break;
            case 2101: if (m_onApplyPreset) m_onApplyPreset(0); break;
            case 2102: if (m_onApplyPreset) m_onApplyPreset(1); break;
            case 2103: if (m_onApplyPreset) m_onApplyPreset(2); break;
            case 2104: if (m_onApplyPreset) m_onApplyPreset(3); break;
            case 2005: if (m_onExit) m_onExit(); break;
            default: break;
        }
    }
#endif

} // namespace Ui
} // namespace Recorder
