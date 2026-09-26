#include "ui/TrayIcon.h"
#include <iostream>

#if defined(_WIN32)
#pragma comment(lib, "shell32.lib")
#endif

namespace Recorder::Ui {

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
        wcscpy_s(m_nid.szTip, L"High-Performance Screen Recorder");

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
        AppendMenuW(hMenu, MF_STRING, 2001, L"Show Screen Recorder");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 2002, L"Start Recording");
        AppendMenuW(hMenu, MF_STRING, 2003, L"Stop Recording");
        AppendMenuW(hMenu, MF_STRING, 2004, L"Save Instant Replay");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 2005, L"Exit");

        SetForegroundWindow(m_hwnd);
        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, x, y, 0, m_hwnd, nullptr);
        DestroyMenu(hMenu);

        switch (cmd) {
            case 2001: if (m_onRestore) m_onRestore(); break;
            case 2002: if (m_onStartRecord) m_onStartRecord(); break;
            case 2003: if (m_onStopRecord) m_onStopRecord(); break;
            case 2004: if (m_onSaveReplay) m_onSaveReplay(); break;
            case 2005: if (m_onExit) m_onExit(); break;
            default: break;
        }
    }
#endif

} // namespace Recorder::Ui
