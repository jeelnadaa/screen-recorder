#pragma once

#include <string>
#include <functional>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

namespace Recorder {
namespace Ui {

    class TrayIcon {
    public:
        TrayIcon();
        ~TrayIcon();

#if defined(_WIN32)
        bool Initialize(HWND hwnd, UINT callbackMessageId, HICON icon = nullptr);
        void Remove();

        void ShowNotification(const std::wstring& title, const std::wstring& message);
        void ShowContextMenu(int x, int y);
#endif

        void SetOnStartRecord(std::function<void()> cb) { m_onStartRecord = cb; }
        void SetOnStopRecord(std::function<void()> cb) { m_onStopRecord = cb; }
        void SetOnSaveReplay(std::function<void()> cb) { m_onSaveReplay = cb; }
        void SetOnScreenshot(std::function<void()> cb) { m_onScreenshot = cb; }
        void SetOnApplyPreset(std::function<void(int)> cb) { m_onApplyPreset = cb; }
        void SetOnRestoreWindow(std::function<void()> cb) { m_onRestore = cb; }
        void SetOnExitApp(std::function<void()> cb) { m_onExit = cb; }

    private:
#if defined(_WIN32)
        NOTIFYICONDATAW m_nid = {};
        HWND m_hwnd = nullptr;
        bool m_added = false;
#endif

        std::function<void()> m_onStartRecord;
        std::function<void()> m_onStopRecord;
        std::function<void()> m_onSaveReplay;
        std::function<void()> m_onScreenshot;
        std::function<void(int)> m_onApplyPreset;
        std::function<void()> m_onRestore;
        std::function<void()> m_onExit;
    };

} // namespace Ui
} // namespace Recorder
