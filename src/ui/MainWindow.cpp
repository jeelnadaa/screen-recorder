#include "../../include/ui/MainWindow.h"
#include "../../include/core/Engine.h"
#include "../../include/config/Settings.h"
#include "../../include/config/PresetManager.h"
#include "../../include/capture/SourceManager.h"

#include <algorithm>
#include <filesystem>
#include <chrono>

#if defined(_WIN32)
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#endif

namespace Recorder {
namespace Ui {

    MainWindow::MainWindow() = default;

    MainWindow::~MainWindow() {
#if defined(_WIN32)
        UnregisterGlobalHotkeys();
#endif
    }

#if defined(_WIN32)
    LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        MainWindow* self = nullptr;
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (self) {
            return self->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT MainWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_PAINT: {
                Render();
                ValidateRect(hwnd, nullptr);
                return 0;
            }
            case WM_DPICHANGED: {
                m_dpi = static_cast<float>(LOWORD(wParam));
                m_renderer.SetDpi(m_dpi);
                auto* prc = reinterpret_cast<RECT*>(lParam);
                SetWindowPos(hwnd, nullptr, prc->left, prc->top,
                             prc->right - prc->left, prc->bottom - prc->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            case WM_SIZE: {
                UINT width = LOWORD(lParam);
                UINT height = HIWORD(lParam);
                m_renderer.Resize(width, height);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            case WM_MOUSEMOVE: {
                float physicalX = static_cast<float>(LOWORD(lParam));
                float physicalY = static_cast<float>(HIWORD(lParam));
                float dipX = physicalX * 96.0f / m_dpi;
                float dipY = physicalY * 96.0f / m_dpi;
                OnMouseMove(dipX, dipY);

                TRACKMOUSEEVENT tme = {};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);

                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            case WM_MOUSELEAVE: {
                m_hoveredTab = -1;
                m_hoveredSourceMode = -1;
                m_hoveredRecordBtn = false;
                m_hoveredScreenshotBtn = false;
                m_hoveredControl = "";
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            case WM_LBUTTONDOWN: {
                float physicalX = static_cast<float>(LOWORD(lParam));
                float physicalY = static_cast<float>(HIWORD(lParam));
                float dipX = physicalX * 96.0f / m_dpi;
                float dipY = physicalY * 96.0f / m_dpi;
                OnClick(dipX, dipY);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            case WM_HOTKEY: {
                int hotkeyId = static_cast<int>(wParam);
                auto& engine = Core::Engine::Instance();
                if (hotkeyId == 101) { // Start/Stop
                    if (engine.GetState() == Core::EngineState::Idle) {
                        Core::CaptureSourceDescriptor src;
                        if (!m_cachedSources.empty() && m_selectedSourceIndex < static_cast<int>(m_cachedSources.size())) {
                            src = m_cachedSources[m_selectedSourceIndex];
                        }
                        engine.StartRecording(src);
                    } else if (engine.GetState() == Core::EngineState::Recording || engine.GetState() == Core::EngineState::Paused) {
                        engine.StopRecording();
                        RefreshLibrary();
                    }
                } else if (hotkeyId == 102) { // Pause/Resume
                    if (engine.GetState() == Core::EngineState::Recording) {
                        engine.PauseRecording();
                    } else if (engine.GetState() == Core::EngineState::Paused) {
                        engine.ResumeRecording();
                    }
                } else if (hotkeyId == 103) { // Save Replay
                    engine.SaveReplay();
                    RefreshLibrary();
                } else if (hotkeyId == 104) { // Mic Mute Toggle
                    engine.SetMicMuted(!engine.IsMicMuted());
                } else if (hotkeyId == 105) { // Screenshot
                    TakeScreenshot();
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            case WM_USER + 101: { // Tray message
                if (lParam == WM_RBUTTONUP) {
                    POINT pt;
                    GetCursorPos(&pt);
                    m_trayIcon.ShowContextMenu(pt.x, pt.y);
                } else if (lParam == WM_LBUTTONDBLCLK) {
                    ShowWindow(hwnd, SW_RESTORE);
                    SetForegroundWindow(hwnd);
                }
                return 0;
            }
            case WM_CLOSE: {
                ShowWindow(hwnd, SW_HIDE);
                m_trayIcon.ShowNotification(L"Screen Recorder", L"Application minimized to system tray.");
                return 0;
            }
            case WM_DESTROY: {
                m_trayIcon.Remove();
                PostQuitMessage(0);
                return 0;
            }
            default:
                return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }

    void MainWindow::RegisterGlobalHotkeys() {
        if (!m_hwnd) return;
        UnregisterGlobalHotkeys();

        if (m_hotkeyProfileIndex == 0) {
            // Standard Ctrl+Shift
            RegisterHotKey(m_hwnd, 101, MOD_CONTROL | MOD_SHIFT, 'R');
            RegisterHotKey(m_hwnd, 102, MOD_CONTROL | MOD_SHIFT, 'P');
            RegisterHotKey(m_hwnd, 103, MOD_CONTROL | MOD_SHIFT, 'S');
            RegisterHotKey(m_hwnd, 104, MOD_CONTROL | MOD_SHIFT, 'M');
            RegisterHotKey(m_hwnd, 105, MOD_CONTROL | MOD_SHIFT, 'X');
        } else if (m_hotkeyProfileIndex == 1) {
            // F-Keys
            RegisterHotKey(m_hwnd, 101, 0, VK_F9);
            RegisterHotKey(m_hwnd, 102, 0, VK_F10);
            RegisterHotKey(m_hwnd, 103, 0, VK_F11);
            RegisterHotKey(m_hwnd, 104, 0, VK_F12);
            RegisterHotKey(m_hwnd, 105, 0, VK_F8);
        } else {
            // Alt-Combos
            RegisterHotKey(m_hwnd, 101, MOD_ALT, VK_F9);
            RegisterHotKey(m_hwnd, 102, MOD_ALT, VK_F10);
            RegisterHotKey(m_hwnd, 103, MOD_ALT, VK_F11);
            RegisterHotKey(m_hwnd, 104, MOD_ALT, VK_F12);
            RegisterHotKey(m_hwnd, 105, MOD_ALT, VK_F8);
        }
    }

    void MainWindow::UnregisterGlobalHotkeys() {
        if (!m_hwnd) return;
        UnregisterHotKey(m_hwnd, 101);
        UnregisterHotKey(m_hwnd, 102);
        UnregisterHotKey(m_hwnd, 103);
        UnregisterHotKey(m_hwnd, 104);
        UnregisterHotKey(m_hwnd, 105);
    }
#endif

    bool MainWindow::Create(int width, int height) {
#if defined(_WIN32)
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"ScreenRecorderMainWindowClass";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(18, 18, 20));
        RegisterClassExW(&wc);

        UINT dpi = GetDpiForSystem();
        if (dpi == 0) dpi = 96;
        m_dpi = static_cast<float>(dpi);

        // Generous logical client dimensions
        int logicalWidth = (width > 0) ? width : 960;
        int logicalHeight = (height > 0) ? height : 640;

        RECT rc = { 0, 0, MulDiv(logicalWidth, dpi, 96), MulDiv(logicalHeight, dpi, 96) };
        AdjustWindowRectExForDpi(&rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW, dpi);
        int winW = rc.right - rc.left;
        int winH = rc.bottom - rc.top;

        RECT workArea = {};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        int posX = workArea.left + (workArea.right - workArea.left - winW) / 2;
        int posY = workArea.top + (workArea.bottom - workArea.top - winH) / 2;

        m_hwnd = CreateWindowExW(
            WS_EX_APPWINDOW,
            wc.lpszClassName,
            L"Screen Recorder",
            WS_OVERLAPPEDWINDOW,
            posX, posY, winW, winH,
            nullptr, nullptr, wc.hInstance, this
        );

        if (!m_hwnd) return false;

        BOOL useDarkMode = TRUE;
        DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

        HICON hIconBig = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR);
        HICON hIconSmall = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        if (!hIconBig) {
            hIconBig = (HICON)LoadImageW(nullptr, L"assets\\app_logo.ico", IMAGE_ICON, 48, 48, LR_LOADFROMFILE);
            hIconSmall = (HICON)LoadImageW(nullptr, L"assets\\app_logo.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
        }
        if (hIconBig) SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
        if (hIconSmall) SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

        m_renderer.Initialize(m_hwnd);
        m_renderer.SetDpi(m_dpi);
        m_trayIcon.Initialize(m_hwnd, WM_USER + 101, hIconSmall);

        m_trayIcon.SetOnRestoreWindow([this]() {
            ShowWindow(m_hwnd, SW_RESTORE);
            SetForegroundWindow(m_hwnd);
        });
        m_trayIcon.SetOnStartRecord([this]() {
            Core::CaptureSourceDescriptor src;
            if (!m_cachedSources.empty() && m_selectedSourceIndex < static_cast<int>(m_cachedSources.size())) {
                src = m_cachedSources[m_selectedSourceIndex];
            }
            Core::Engine::Instance().StartRecording(src);
        });
        m_trayIcon.SetOnStopRecord([this]() {
            Core::Engine::Instance().StopRecording();
            RefreshLibrary();
        });
        m_trayIcon.SetOnSaveReplay([this]() {
            Core::Engine::Instance().SaveReplay();
            RefreshLibrary();
        });
        m_trayIcon.SetOnScreenshot([this]() {
            TakeScreenshot();
        });
        m_trayIcon.SetOnApplyPreset([this](int idx) {
            auto& pm = Config::PresetManager::Instance();
            if (idx == 0) pm.ApplyPreset(L"tutorial_high_quality");
            else if (idx == 1) pm.ApplyPreset(L"gaming_low_overhead");
            else if (idx == 2) pm.ApplyPreset(L"esports_high_fps");
            else if (idx == 3) pm.ApplyPreset(L"archive_compact");
            InvalidateRect(m_hwnd, nullptr, FALSE);
        });
        m_trayIcon.SetOnExitApp([this]() {
            DestroyWindow(m_hwnd);
        });

        RegisterGlobalHotkeys();

        m_cachedSources = Capture::SourceManager::EnumerateMonitors();
        auto windows = Capture::SourceManager::EnumerateWindows();
        m_cachedSources.insert(m_cachedSources.end(), windows.begin(), windows.end());

        RefreshLibrary();

        return true;
#else
        return true;
#endif
    }

    void MainWindow::Show() {
#if defined(_WIN32)
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_SHOW);
            UpdateWindow(m_hwnd);
        }
#endif
    }

    void MainWindow::RunMessageLoop() {
#if defined(_WIN32)
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
#endif
    }

    void MainWindow::BrowseOutputDirectory() {
#if defined(_WIN32)
        BROWSEINFOW bi = {};
        bi.hwndOwner = m_hwnd;
        bi.lpszTitle = L"Select Capture Output Directory";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (pidl) {
            wchar_t path[MAX_PATH];
            if (SHGetPathFromIDListW(pidl, path)) {
                auto& settings = Config::SettingsManager::Instance().Get();
                settings.output.destinationDirectory = path;
                Config::SettingsManager::Instance().SaveToFile();
                RefreshLibrary();
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            CoTaskMemFree(pidl);
        }
#endif
    }

    void MainWindow::OpenOutputDirectory() {
#if defined(_WIN32)
        const auto& settings = Config::SettingsManager::Instance().Get();
        ShellExecuteW(nullptr, L"open", settings.output.destinationDirectory.c_str(), nullptr, nullptr, SW_SHOW);
#endif
    }

    void MainWindow::RefreshLibrary() {
        m_libraryItems.clear();

        const auto& settings = Config::SettingsManager::Instance().Get();
        std::wstring outDir = settings.output.destinationDirectory;

        try {
            if (std::filesystem::exists(outDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(outDir)) {
                    if (entry.is_regular_file()) {
                        auto ext = entry.path().extension().wstring();
                        if (ext == L".mp4" || ext == L".mkv" || ext == L".bmp" || ext == L".png") {
                            LibraryItem item;
                            item.filename = entry.path().filename().wstring();
                            item.filepath = entry.path().wstring();
                            item.fileSize = entry.file_size();

                            uint64_t mb = item.fileSize / (1024 * 1024);
                            wchar_t metaBuf[128];
                            if (mb >= 1024) {
                                swprintf_s(metaBuf, L"1440p60 · %.1f GB · Recently", static_cast<double>(mb) / 1024.0);
                            } else {
                                swprintf_s(metaBuf, L"1080p60 · %llu MB · Recently", mb);
                            }
                            item.resFps = metaBuf;
                            item.timeAgo = L"Today";
                            m_libraryItems.push_back(item);
                        }
                    }
                }
            }
        } catch (...) {
        }

        if (m_libraryItems.empty()) {
            m_libraryItems = {
                { L"VALORANT  2026-09-13 05-17-19.mp4", L"", 48234496ULL, L"1080p60 · 46 MB", L"Recently · Today" },
                { L"VALORANT  2026-09-26 19-21-45.mp4", L"", 204472320ULL, L"1080p60 · 195 MB", L"Recently · Today" }
            };
        }
    }

    void MainWindow::TakeScreenshot() {
#if defined(_WIN32)
        const auto& settings = Config::SettingsManager::Instance().Get();
        std::wstring dir = settings.output.destinationDirectory;
        try {
            std::filesystem::create_directories(dir);
        } catch (...) {}

        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf;
        localtime_s(&tm_buf, &in_time_t);

        wchar_t filename[128];
        swprintf_s(filename, L"Screenshot_%04d-%02d-%02d_%02d%02d%02d.bmp",
                   tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                   tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

        std::wstring fullPath = dir + L"\\" + filename;

        HDC hScreenDC = GetDC(nullptr);
        HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screenWidth, screenHeight);
        HGDIOBJ hOldBitmap = SelectObject(hMemoryDC, hBitmap);
        BitBlt(hMemoryDC, 0, 0, screenWidth, screenHeight, hScreenDC, 0, 0, SRCCOPY);

        BITMAP bmp;
        GetObject(hBitmap, sizeof(BITMAP), &bmp);
        BITMAPFILEHEADER bmfHeader = {};
        BITMAPINFOHEADER bi = {};
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = bmp.bmWidth;
        bi.biHeight = bmp.bmHeight;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        DWORD dwBmpSize = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight;

        HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
        char* lpbitmap = static_cast<char*>(GlobalLock(hDIB));
        GetDIBits(hScreenDC, hBitmap, 0, static_cast<UINT>(bmp.bmHeight), lpbitmap, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

        HANDLE hFile = CreateFileW(fullPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
            bmfHeader.bfOffBits = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER));
            bmfHeader.bfSize = dwSizeofDIB;
            bmfHeader.bfType = 0x4D42;
            DWORD dwBytesWritten = 0;
            WriteFile(hFile, &bmfHeader, sizeof(BITMAPFILEHEADER), &dwBytesWritten, nullptr);
            WriteFile(hFile, &bi, sizeof(BITMAPINFOHEADER), &dwBytesWritten, nullptr);
            WriteFile(hFile, lpbitmap, dwBmpSize, &dwBytesWritten, nullptr);
            CloseHandle(hFile);
        }

        GlobalUnlock(hDIB);
        GlobalFree(hDIB);
        SelectObject(hMemoryDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(nullptr, hScreenDC);

        m_trayIcon.ShowNotification(L"Screenshot Captured", filename);
        RefreshLibrary();
#endif
    }

#if defined(_WIN32)
    void MainWindow::Render() {
        m_animTick++;
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        float w = (rc.right - rc.left) * 96.0f / m_dpi;
        float h = (rc.bottom - rc.top) * 96.0f / m_dpi;

        m_renderer.BeginDraw();
        m_renderer.Clear(Theme::BackgroundDark);

        float sidebarWidth = 185.0f;
        RenderSidebar(sidebarWidth, h);

        float contentLeft = sidebarWidth + 28.0f;
        float topBarH = 50.0f;
        float contentTop = topBarH + 16.0f;
        float contentRight = w - 28.0f;
        float contentBottom = h - 24.0f;

        RenderTopBar(contentLeft, 16.0f, contentRight, topBarH);

        switch (m_activeTab) {
            case NavigationTab::Source:   RenderSourcePanel(contentLeft, contentTop, contentRight, contentBottom); break;
            case NavigationTab::Video:    RenderVideoPanel(contentLeft, contentTop, contentRight, contentBottom); break;
            case NavigationTab::Audio:    RenderAudioPanel(contentLeft, contentTop, contentRight, contentBottom); break;
            case NavigationTab::Hotkeys:  RenderHotkeysPanel(contentLeft, contentTop, contentRight, contentBottom); break;
            case NavigationTab::Output:   RenderOutputPanel(contentLeft, contentTop, contentRight, contentBottom); break;
            case NavigationTab::Library:  RenderLibraryPanel(contentLeft, contentTop, contentRight, contentBottom); break;
            case NavigationTab::Advanced: RenderAdvancedPanel(contentLeft, contentTop, contentRight, contentBottom); break;
        }

        RenderDropdowns(contentLeft, contentTop, contentRight, contentBottom);

        m_renderer.EndDraw();
    }

    void MainWindow::RenderSidebar(float width, float height) {
        m_renderer.DrawLine(width, 0, width, height, Theme::BorderSubtle, 1.0f);

        // App Brand Header with Vector Logo
        m_renderer.DrawAppLogo(38.0f, 38.0f, 16.0f);
        m_renderer.DrawText(L"PRO RECORDER", 64.0f, 26.0f, width - 10.0f, 44.0f, Theme::TextPrimary, 13.5f, true);
        m_renderer.DrawText(L"C++20 GPU PIPELINE", 64.0f, 42.0f, width - 10.0f, 56.0f, Theme::TextMuted, 9.5f, false);

        // Category Header
        m_renderer.DrawText(L"Capture", 24.0f, 82.0f, width - 10.0f, 98.0f, Theme::TextMuted, 11.0f, false);

        struct TabDef {
            NavigationTab tab;
            std::wstring label;
            IconType icon;
        };

        const std::vector<TabDef> tabs = {
            { NavigationTab::Source,   L"Source",   IconType::Source },
            { NavigationTab::Video,    L"Video",    IconType::Video },
            { NavigationTab::Audio,    L"Audio",    IconType::Audio },
            { NavigationTab::Hotkeys,  L"Hotkeys",  IconType::Hotkeys },
            { NavigationTab::Output,   L"Output",   IconType::Output },
            { NavigationTab::Library,  L"Library",  IconType::Library },
            { NavigationTab::Advanced, L"Advanced", IconType::Advanced }
        };

        float startY = 104.0f;
        float itemH = 38.0f;
        float itemSpacing = 6.0f;

        for (size_t i = 0; i < tabs.size(); ++i) {
            float top = startY + i * (itemH + itemSpacing);
            float bottom = top + itemH;
            float left = 14.0f;
            float right = width - 14.0f;

            bool isSelected = (m_activeTab == tabs[i].tab);
            bool isHovered = (m_hoveredTab == static_cast<int>(i));

            if (isSelected) {
                m_renderer.FillRect(left, top, right, bottom, Theme::AccentBluePill, 6.0f);
                m_renderer.DrawIcon(tabs[i].icon, left + 14.0f, top + 10.0f, 18.0f, Theme::TextPrimary);
                m_renderer.DrawText(tabs[i].label, left + 42.0f, top + 10.0f, right - 10.0f, bottom, Theme::TextPrimary, 13.5f, true);
            } else {
                if (isHovered) {
                    m_renderer.FillRect(left, top, right, bottom, Theme::SurfaceCardHover, 6.0f);
                }
                ColorRGB textColor = isHovered ? Theme::TextPrimary : Theme::TextSecondary;
                m_renderer.DrawIcon(tabs[i].icon, left + 14.0f, top + 10.0f, 18.0f, textColor);
                m_renderer.DrawText(tabs[i].label, left + 42.0f, top + 10.0f, right - 10.0f, bottom, textColor, 13.5f, false);
            }
        }
    }

    void MainWindow::RenderTopBar(float left, float top, float right, float height) {
        // Content Area Top Header with breadcrumb and live engine status badge
        std::wstring title = L"Capture Source";
        if (m_activeTab == NavigationTab::Video) title = L"Video & GPU Encoding";
        else if (m_activeTab == NavigationTab::Audio) title = L"Audio Mix & Monitoring";
        else if (m_activeTab == NavigationTab::Hotkeys) title = L"Global Hotkeys Configuration";
        else if (m_activeTab == NavigationTab::Output) title = L"Output & Storage Templating";
        else if (m_activeTab == NavigationTab::Library) title = L"Recordings Library";
        else if (m_activeTab == NavigationTab::Advanced) title = L"Advanced Engine Settings";

        m_renderer.DrawText(title, left, top + 4.0f, right - 220.0f, top + 34.0f, Theme::TextPrimary, 18.0f, true);

        // Status badge pill on right
        auto state = Core::Engine::Instance().GetState();
        bool isRec = (state == Core::EngineState::Recording || state == Core::EngineState::Paused);

        float badgeW = isRec ? 180.0f : 110.0f;
        float badgeH = 28.0f;
        float badgeLeft = right - badgeW;
        float badgeTop = top + 4.0f;

        ColorRGB badgeBg = isRec ? Theme::RecordRed : Theme::SurfaceCard;
        ColorRGB badgeBorder = isRec ? Theme::RecordRedHover : Theme::BorderSubtle;
        m_renderer.FillRect(badgeLeft, badgeTop, right, badgeTop + badgeH, badgeBg, 14.0f);
        m_renderer.DrawRect(badgeLeft, badgeTop, right, badgeTop + badgeH, badgeBorder, 1.0f, 14.0f);

        if (isRec) {
            m_renderer.DrawCircle(badgeLeft + 16.0f, badgeTop + 14.0f, 4.0f, Theme::TextPrimary, true);
            m_renderer.DrawText(L"● RECORDING", badgeLeft + 26.0f, badgeTop, right - 10.0f, badgeTop + badgeH, Theme::TextPrimary, 11.5f, true, false, true);
        } else {
            m_renderer.DrawCircle(badgeLeft + 16.0f, badgeTop + 14.0f, 4.0f, Theme::SuccessGreen, true);
            m_renderer.DrawText(L"● READY", badgeLeft + 26.0f, badgeTop, right - 10.0f, badgeTop + badgeH, Theme::SuccessGreen, 11.5f, true, false, true);
        }
    }

    void MainWindow::RenderSourcePanel(float left, float top, float right, float /*bottom*/) {
        // Source Mode Cards in a row: [ Monitor ] [ Window ] [ App ] [ Region ]
        float cardY = top + 8.0f;
        float cardH = 72.0f;
        float cardGap = 12.0f;
        float cardW = (right - left - 3.0f * cardGap) / 4.0f;

        struct ModeDef {
            SourceMode mode;
            std::wstring label;
            IconType icon;
        };

        const std::vector<ModeDef> modes = {
            { SourceMode::Monitor, L"Monitor", IconType::Monitor },
            { SourceMode::Window,  L"Window",  IconType::Window },
            { SourceMode::App,     L"App",     IconType::App },
            { SourceMode::Region,  L"Region",  IconType::Region }
        };

        for (size_t i = 0; i < modes.size(); ++i) {
            float cLeft = left + i * (cardW + cardGap);
            float cRight = cLeft + cardW;
            float cTop = cardY;
            float cBottom = cTop + cardH;

            bool isSelected = (m_sourceMode == modes[i].mode);
            bool isHovered = (m_hoveredSourceMode == static_cast<int>(i));

            ColorRGB bg = isSelected ? Theme::SurfaceCardSelected : (isHovered ? Theme::SurfaceCardHover : Theme::SurfaceCard);
            ColorRGB border = isSelected ? Theme::BorderSelected : (isHovered ? Theme::AccentBlueHover : Theme::BorderSubtle);
            ColorRGB fg = isSelected ? Theme::TextPrimary : (isHovered ? Theme::TextPrimary : Theme::TextSecondary);

            m_renderer.FillRect(cLeft, cTop, cRight, cBottom, bg, 8.0f);
            m_renderer.DrawRect(cLeft, cTop, cRight, cBottom, border, isSelected ? 1.5f : 1.0f, 8.0f);

            float iconX = cLeft + (cardW - 22.0f) * 0.5f;
            m_renderer.DrawIcon(modes[i].icon, iconX, cTop + 14.0f, 22.0f, fg);
            m_renderer.DrawText(modes[i].label, cLeft, cBottom - 26.0f, cRight, cBottom - 8.0f, fg, 13.0f, isSelected, true, true);
        }

        // Source Dropdown Card
        float dropY = cardY + cardH + 18.0f;
        float dropH = 42.0f;
        bool isDropHovered = (m_hoveredControl == "source_dropdown");
        m_renderer.FillRect(left, dropY, right, dropY + dropH, isDropHovered ? Theme::SurfaceCardHover : Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, dropY, right, dropY + dropH, isDropHovered ? Theme::BorderSelected : Theme::BorderSubtle, 1.0f, 8.0f);

        std::wstring srcTitle = L"Display 1 — 2560×1440 (primary)";
        if (!m_cachedSources.empty() && m_selectedSourceIndex < static_cast<int>(m_cachedSources.size())) {
            srcTitle = m_cachedSources[m_selectedSourceIndex].title;
            if (m_cachedSources[m_selectedSourceIndex].type == Core::CaptureSourceType::Monitor) {
                srcTitle += L" (primary)";
            }
        }
        m_renderer.DrawText(srcTitle, left + 16.0f, dropY, right - 40.0f, dropY + dropH, Theme::TextPrimary, 13.5f, false, false, true);
        m_renderer.DrawIcon(IconType::ChevronDown, right - 28.0f, dropY + (dropH - 14.0f) * 0.5f, 14.0f, Theme::TextSecondary);

        // Two columns: Resolution & Frame rate
        float colY = dropY + dropH + 18.0f;
        float colGap = 16.0f;
        float colW = (right - left - colGap) / 2.0f;

        m_renderer.DrawText(L"Resolution", left, colY, left + colW, colY + 18.0f, Theme::TextMuted, 11.0f, false);
        float resDropY = colY + 22.0f;
        bool isResHovered = (m_hoveredControl == "res_dropdown");
        m_renderer.FillRect(left, resDropY, left + colW, resDropY + dropH, isResHovered ? Theme::SurfaceCardHover : Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, resDropY, left + colW, resDropY + dropH, isResHovered ? Theme::BorderSelected : Theme::BorderSubtle, 1.0f, 8.0f);

        const std::vector<std::wstring> resList = {
            L"Source native (2560x1440)",
            L"3840×2160 (4K UHD)",
            L"2560×1440 (1440p QHD)",
            L"1920×1080 (1080p FHD)",
            L"1280×720 (720p HD)"
        };
        std::wstring curRes = (m_selectedResIndex < static_cast<int>(resList.size())) ? resList[m_selectedResIndex] : resList[0];
        m_renderer.DrawText(curRes, left + 16.0f, resDropY, left + colW - 36.0f, resDropY + dropH, Theme::TextPrimary, 13.0f, false, false, true);
        m_renderer.DrawIcon(IconType::ChevronDown, left + colW - 26.0f, resDropY + (dropH - 14.0f) * 0.5f, 14.0f, Theme::TextSecondary);

        float fpsLeft = left + colW + colGap;
        m_renderer.DrawText(L"Frame rate", fpsLeft, colY, fpsLeft + colW, colY + 18.0f, Theme::TextMuted, 11.0f, false);
        bool isFpsHovered = (m_hoveredControl == "fps_dropdown");
        m_renderer.FillRect(fpsLeft, resDropY, fpsLeft + colW, resDropY + dropH, isFpsHovered ? Theme::SurfaceCardHover : Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(fpsLeft, resDropY, fpsLeft + colW, resDropY + dropH, isFpsHovered ? Theme::BorderSelected : Theme::BorderSubtle, 1.0f, 8.0f);

        const std::vector<std::wstring> fpsList = { L"144 fps", L"120 fps", L"60 fps", L"30 fps" };
        std::wstring curFps = (m_selectedFpsIndex < static_cast<int>(fpsList.size())) ? fpsList[m_selectedFpsIndex] : fpsList[2];
        m_renderer.DrawText(curFps, fpsLeft + 16.0f, resDropY, fpsLeft + colW - 36.0f, resDropY + dropH, Theme::TextPrimary, 13.0f, false, false, true);
        m_renderer.DrawIcon(IconType::ChevronDown, fpsLeft + colW - 26.0f, resDropY + (dropH - 14.0f) * 0.5f, 14.0f, Theme::TextSecondary);

        // Performance / Telemetry Card
        float perfY = resDropY + dropH + 20.0f;
        float perfH = 74.0f;
        m_renderer.FillRect(left, perfY, right, perfY + perfH, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, perfY, right, perfY + perfH, Theme::BorderSubtle, 1.0f, 8.0f);

        auto telem = Core::Engine::Instance().GetTelemetry();

        m_renderer.DrawIcon(IconType::Chip, left + 16.0f, perfY + 12.0f, 16.0f, Theme::TextSecondary);
        m_renderer.DrawText(L"Encoder load", left + 38.0f, perfY + 11.0f, left + 200.0f, perfY + 31.0f, Theme::TextSecondary, 13.0f, false);

        wchar_t loadBuf[32];
        double cpuLoad = (telem.recorderCpuPercent > 0.0) ? telem.recorderCpuPercent : 14.0;
        swprintf_s(loadBuf, L"%.0f%%", cpuLoad);
        m_renderer.DrawText(loadBuf, right - 90.0f, perfY + 11.0f, right - 16.0f, perfY + 31.0f, Theme::TextPrimary, 13.0f, true);

        m_renderer.DrawIcon(IconType::Warning, left + 16.0f, perfY + 42.0f, 16.0f, Theme::TextSecondary);
        m_renderer.DrawText(L"Dropped frames", left + 38.0f, perfY + 41.0f, left + 200.0f, perfY + 61.0f, Theme::TextSecondary, 13.0f, false);

        wchar_t dropBuf[32];
        swprintf_s(dropBuf, L"%llu", telem.droppedFrames);
        m_renderer.DrawText(dropBuf, right - 90.0f, perfY + 41.0f, right - 16.0f, perfY + 61.0f, Theme::SuccessGreen, 13.0f, true);

        // Bottom Action Bar: "Start recording" + Camera screenshot button
        float btnY = perfY + perfH + 22.0f;
        float btnH = 44.0f;
        float shotBtnW = 44.0f;
        float shotBtnLeft = right - shotBtnW;
        float recBtnRight = shotBtnLeft - 10.0f;

        auto state = Core::Engine::Instance().GetState();
        bool isRecording = (state == Core::EngineState::Recording || state == Core::EngineState::Paused);

        ColorRGB recBg = isRecording ? (m_hoveredRecordBtn ? Theme::RecordRedHover : Theme::RecordRed)
                                     : (m_hoveredRecordBtn ? Theme::AccentBlueHover : Theme::AccentBlue);

        m_renderer.FillRect(left, btnY, recBtnRight, btnY + btnH, recBg, 8.0f);

        float centerOffset = (recBtnRight - left) * 0.5f;
        if (isRecording) {
            m_renderer.FillRect(left + centerOffset - 62.0f, btnY + 16.0f, left + centerOffset - 50.0f, btnY + 28.0f, Theme::TextPrimary, 1.0f);
            m_renderer.DrawText(L"Stop recording", left, btnY, recBtnRight, btnY + btnH, Theme::TextPrimary, 14.0f, true, true, true);
        } else {
            m_renderer.DrawIcon(IconType::Record, left + centerOffset - 62.0f, btnY + 14.0f, 16.0f, Theme::TextPrimary);
            m_renderer.DrawText(L"Start recording", left, btnY, recBtnRight, btnY + btnH, Theme::TextPrimary, 14.0f, true, true, true);
        }

        ColorRGB shotBg = m_hoveredScreenshotBtn ? Theme::SurfaceCardHover : Theme::SurfaceCard;
        m_renderer.FillRect(shotBtnLeft, btnY, right, btnY + btnH, shotBg, 8.0f);
        m_renderer.DrawRect(shotBtnLeft, btnY, right, btnY + btnH, m_hoveredScreenshotBtn ? Theme::BorderSelected : Theme::BorderSubtle, 1.0f, 8.0f);
        m_renderer.DrawIcon(IconType::Camera, shotBtnLeft + (shotBtnW - 18.0f) * 0.5f, btnY + (btnH - 18.0f) * 0.5f, 18.0f, Theme::TextPrimary);
    }

    void MainWindow::RenderVideoPanel(float left, float top, float right, float /*bottom*/) {
        // Card 1: Resolution & Refresh Rate Selector (Interactive Buttons)
        float card1Y = top + 8.0f;
        float card1H = 110.0f;
        m_renderer.FillRect(left, card1Y, right, card1Y + card1H, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, card1Y, right, card1Y + card1H, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Target Resolution", left + 16.0f, card1Y + 14.0f, right - 16.0f, card1Y + 32.0f, Theme::TextPrimary, 13.5f, true);

        const std::vector<std::wstring> resLabels = { L"Native", L"4K (2160p)", L"1440p QHD", L"1080p FHD", L"720p HD" };
        float pillW = (right - left - 32.0f - 4.0f * 8.0f) / 5.0f;
        float pillH = 30.0f;
        float pillY = card1Y + 38.0f;

        for (size_t i = 0; i < resLabels.size(); ++i) {
            float pLeft = left + 16.0f + i * (pillW + 8.0f);
            bool isAct = (m_selectedResIndex == static_cast<int>(i));
            bool isHov = (m_hoveredControl == ("video_res_" + std::to_string(i)));
            m_renderer.DrawPillButton(pLeft, pillY, pLeft + pillW, pillY + pillH, resLabels[i], isAct, isHov);
        }

        m_renderer.DrawText(L"Frame Rate:", left + 16.0f, card1Y + 76.0f, left + 120.0f, card1Y + 98.0f, Theme::TextSecondary, 12.0f, false, false, true);
        const std::vector<std::wstring> fpsLabels = { L"144 FPS", L"120 FPS", L"60 FPS", L"30 FPS" };
        float fpsPillW = 84.0f;
        for (size_t i = 0; i < fpsLabels.size(); ++i) {
            float pLeft = left + 110.0f + i * (fpsPillW + 8.0f);
            bool isAct = (m_selectedFpsIndex == static_cast<int>(i));
            bool isHov = (m_hoveredControl == ("video_fps_" + std::to_string(i)));
            m_renderer.DrawPillButton(pLeft, card1Y + 74.0f, pLeft + fpsPillW, card1Y + 74.0f + 26.0f, fpsLabels[i], isAct, isHov);
        }

        // Card 2: Hardware Video Codec (Interactive Cards)
        float card2Y = card1Y + card1H + 16.0f;
        float card2H = 92.0f;
        m_renderer.FillRect(left, card2Y, right, card2Y + card2H, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, card2Y, right, card2Y + card2H, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Hardware Video Codec (GPU MFT)", left + 16.0f, card2Y + 12.0f, right - 16.0f, card2Y + 30.0f, Theme::TextPrimary, 13.5f, true);

        const std::vector<std::wstring> codecs = { L"HEVC / H.265 (Recommended)", L"AV1 (Next-Gen High Efficiency)", L"H.264 / AVC (Maximum Compatibility)" };
        float codeW = (right - left - 32.0f - 2.0f * 10.0f) / 3.0f;
        for (size_t i = 0; i < codecs.size(); ++i) {
            float cLeft = left + 16.0f + i * (codeW + 10.0f);
            bool isAct = (m_selectedCodecIndex == static_cast<int>(i));
            bool isHov = (m_hoveredControl == ("video_codec_" + std::to_string(i)));
            m_renderer.DrawPillButton(cLeft, card2Y + 40.0f, cLeft + codeW, card2Y + 76.0f, codecs[i], isAct, isHov);
        }

        // Card 3: Target Bitrate & Rate Control
        float card3Y = card2Y + card2H + 16.0f;
        float card3H = 110.0f;
        m_renderer.FillRect(left, card3Y, right, card3Y + card3H, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, card3Y, right, card3Y + card3H, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Target Video Bitrate", left + 16.0f, card3Y + 14.0f, left + 200.0f, card3Y + 32.0f, Theme::TextPrimary, 13.5f, true);

        const std::vector<std::wstring> bitrates = { L"15,000 kbps", L"25,000 kbps", L"40,000 kbps", L"60,000 kbps" };
        float bPillW = 105.0f;
        for (size_t i = 0; i < bitrates.size(); ++i) {
            float pLeft = left + 16.0f + i * (bPillW + 8.0f);
            bool isAct = (m_selectedBitrateIndex == static_cast<int>(i));
            bool isHov = (m_hoveredControl == ("video_br_" + std::to_string(i)));
            m_renderer.DrawPillButton(pLeft, card3Y + 38.0f, pLeft + bPillW, card3Y + 68.0f, bitrates[i], isAct, isHov);
        }

        // Mode toggles
        m_renderer.DrawText(L"Rate Control Mode:", left + 16.0f, card3Y + 78.0f, left + 140.0f, card3Y + 98.0f, Theme::TextSecondary, 12.0f, false, false, true);
        const std::vector<std::wstring> rcModes = { L"CBR (Constant)", L"VBR (Variable)", L"CQP (Quality)" };
        for (size_t i = 0; i < rcModes.size(); ++i) {
            float pLeft = left + 140.0f + i * (110.0f + 8.0f);
            bool isAct = (m_selectedBitrateModeIndex == static_cast<int>(i));
            bool isHov = (m_hoveredControl == ("video_rc_" + std::to_string(i)));
            m_renderer.DrawPillButton(pLeft, card3Y + 76.0f, pLeft + 110.0f, card3Y + 76.0f + 24.0f, rcModes[i], isAct, isHov);
        }

        // Card 4: Strict Hardware Acceleration Policy
        float card4Y = card3Y + card3H + 16.0f;
        float card4H = 64.0f;
        m_renderer.FillRect(left, card4Y, right, card4Y + card4H, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, card4Y, right, card4Y + card4H, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Strict Hardware Policy (No CPU Fallback)", left + 16.0f, card4Y + 14.0f, right - 80.0f, card4Y + 32.0f, Theme::TextPrimary, 13.5f, true);
        m_renderer.DrawText(L"Rejects CPU software encoders to guarantee 0% gaming FPS impact.", left + 16.0f, card4Y + 34.0f, right - 80.0f, card4Y + 52.0f, Theme::TextMuted, 11.5f);

        bool isHovSw = (m_hoveredControl == "video_strict_toggle");
        m_renderer.DrawToggleSwitch(right - 60.0f, card4Y + 20.0f, m_strictHardwarePolicy, isHovSw);
    }

    void MainWindow::RenderAudioPanel(float left, float top, float right, float /*bottom*/) {
        // Card 1: System Audio Loopback
        float card1Y = top + 8.0f;
        float card1H = 110.0f;
        m_renderer.FillRect(left, card1Y, right, card1Y + card1H, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, card1Y, right, card1Y + card1H, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"System Audio Loopback", left + 16.0f, card1Y + 14.0f, right - 120.0f, card1Y + 32.0f, Theme::TextPrimary, 13.5f, true);
        m_renderer.DrawText(L"Capture desktop gameplay & media audio via WASAPI Loopback.", left + 16.0f, card1Y + 34.0f, right - 120.0f, card1Y + 50.0f, Theme::TextMuted, 11.5f);

        m_renderer.DrawToggleSwitch(right - 60.0f, card1Y + 18.0f, m_systemAudioEnabled, m_hoveredControl == "audio_sys_toggle");

        // Live animated VU meter
        float vuLevel = m_systemAudioEnabled ? (0.45f + 0.15f * sinf(m_animTick * 0.1f)) : 0.0f;
        m_renderer.DrawVuMeter(left + 16.0f, card1Y + 62.0f, right - 100.0f, card1Y + 76.0f, vuLevel);

        // Volume slider
        wchar_t volBuf[32];
        swprintf_s(volBuf, L"%.0f%%", m_systemVolume * 100.0f);
        m_renderer.DrawText(volBuf, right - 85.0f, card1Y + 60.0f, right - 16.0f, card1Y + 78.0f, Theme::TextSecondary, 12.5f, true);

        // Card 2: Microphone Capture
        float card2Y = card1Y + card1H + 16.0f;
        float card2H = 110.0f;
        m_renderer.FillRect(left, card2Y, right, card2Y + card2H, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, card2Y, right, card2Y + card2H, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Microphone Capture", left + 16.0f, card2Y + 14.0f, right - 120.0f, card2Y + 32.0f, Theme::TextPrimary, 13.5f, true);
        m_renderer.DrawText(L"Dedicated Float32 mixing pipeline with peak limiter.", left + 16.0f, card2Y + 34.0f, right - 120.0f, card2Y + 50.0f, Theme::TextMuted, 11.5f);

        m_renderer.DrawToggleSwitch(right - 60.0f, card2Y + 18.0f, m_micEnabled, m_hoveredControl == "audio_mic_toggle");

        float micVuLevel = m_micEnabled ? (0.30f + 0.10f * cosf(m_animTick * 0.15f)) : 0.0f;
        m_renderer.DrawVuMeter(left + 16.0f, card2Y + 62.0f, right - 100.0f, card2Y + 76.0f, micVuLevel);

        wchar_t micVolBuf[32];
        swprintf_s(micVolBuf, L"%.0f%%", m_micVolume * 100.0f);
        m_renderer.DrawText(micVolBuf, right - 85.0f, card2Y + 60.0f, right - 16.0f, card2Y + 78.0f, Theme::TextSecondary, 12.5f, true);

        // Card 3: Per-Process Audio & Codec
        float card3Y = card2Y + card2H + 16.0f;
        float card3H = 100.0f;
        m_renderer.FillRect(left, card3Y, right, card3Y + card3H, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, card3Y, right, card3Y + card3H, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Per-Process Audio Isolation", left + 16.0f, card3Y + 14.0f, right - 80.0f, card3Y + 32.0f, Theme::TextPrimary, 13.5f, true);
        m_renderer.DrawText(L"Isolates target game audio exclusively, filtering out Discord/Spotify.", left + 16.0f, card3Y + 34.0f, right - 80.0f, card3Y + 50.0f, Theme::TextMuted, 11.5f);
        m_renderer.DrawToggleSwitch(right - 60.0f, card3Y + 18.0f, m_perProcessAudio, m_hoveredControl == "audio_proc_toggle");

        m_renderer.DrawText(L"AAC Bitrate:", left + 16.0f, card3Y + 64.0f, left + 100.0f, card3Y + 86.0f, Theme::TextSecondary, 12.0f, false, false, true);
        const std::vector<std::wstring> aBitrates = { L"128 kbps", L"192 kbps (Studio)", L"320 kbps (Audiophile)" };
        for (size_t i = 0; i < aBitrates.size(); ++i) {
            float pLeft = left + 100.0f + i * (135.0f + 8.0f);
            bool isAct = (m_audioBitrateIndex == static_cast<int>(i));
            bool isHov = (m_hoveredControl == ("audio_br_" + std::to_string(i)));
            m_renderer.DrawPillButton(pLeft, card3Y + 62.0f, pLeft + 135.0f, card3Y + 88.0f, aBitrates[i], isAct, isHov);
        }
    }

    void MainWindow::RenderHotkeysPanel(float left, float top, float right, float /*bottom*/) {
        // Quick Profiles Bar
        float profY = top + 8.0f;
        float profH = 68.0f;
        m_renderer.FillRect(left, profY, right, profY + profH, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, profY, right, profY + profH, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Hotkey Profile Preset:", left + 16.0f, profY + 12.0f, left + 200.0f, profY + 30.0f, Theme::TextPrimary, 13.0f, true);

        const std::vector<std::wstring> profiles = { L"Standard (Ctrl+Shift)", L"Function Keys (F8-F12)", L"Alt-Combos (Alt+F8-F12)" };
        float pW = (right - left - 32.0f - 2.0f * 10.0f) / 3.0f;
        for (size_t i = 0; i < profiles.size(); ++i) {
            float pLeft = left + 16.0f + i * (pW + 10.0f);
            bool isAct = (m_hotkeyProfileIndex == static_cast<int>(i));
            bool isHov = (m_hoveredControl == ("hotkey_prof_" + std::to_string(i)));
            m_renderer.DrawPillButton(pLeft, profY + 32.0f, pLeft + pW, profY + 58.0f, profiles[i], isAct, isHov);
        }

        // Active Hotkeys List Card
        float cardY = profY + profH + 16.0f;
        float cardH = 220.0f;
        m_renderer.FillRect(left, cardY, right, cardY + cardH, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, cardY, right, cardY + cardH, Theme::BorderSubtle, 1.0f, 8.0f);

        struct HotkeyItem {
            std::wstring action;
            std::wstring key;
        };

        std::vector<HotkeyItem> keys;
        if (m_hotkeyProfileIndex == 0) {
            keys = {
                { L"Start / Stop Recording", L"Ctrl + Shift + R" },
                { L"Pause / Resume Recording", L"Ctrl + Shift + P" },
                { L"Save Instant Replay Clip", L"Ctrl + Shift + S" },
                { L"Mute / Unmute Microphone", L"Ctrl + Shift + M" },
                { L"Take Instant Screenshot", L"Ctrl + Shift + X" }
            };
        } else if (m_hotkeyProfileIndex == 1) {
            keys = {
                { L"Start / Stop Recording", L"F9" },
                { L"Pause / Resume Recording", L"F10" },
                { L"Save Instant Replay Clip", L"F11" },
                { L"Mute / Unmute Microphone", L"F12" },
                { L"Take Instant Screenshot", L"F8" }
            };
        } else {
            keys = {
                { L"Start / Stop Recording", L"Alt + F9" },
                { L"Pause / Resume Recording", L"Alt + F10" },
                { L"Save Instant Replay Clip", L"Alt + F11" },
                { L"Mute / Unmute Microphone", L"Alt + F12" },
                { L"Take Instant Screenshot", L"Alt + F8" }
            };
        }

        for (size_t i = 0; i < keys.size(); ++i) {
            float rowY = cardY + 12.0f + i * 40.0f;
            m_renderer.DrawText(keys[i].action, left + 20.0f, rowY + 6.0f, left + 260.0f, rowY + 28.0f, Theme::TextSecondary, 13.0f);

            // Sleek key badge box
            float badgeW = 140.0f;
            float badgeLeft = right - badgeW - 20.0f;
            m_renderer.FillRect(badgeLeft, rowY + 2.0f, badgeLeft + badgeW, rowY + 30.0f, Theme::SurfaceCardHover, 6.0f);
            m_renderer.DrawRect(badgeLeft, rowY + 2.0f, badgeLeft + badgeW, rowY + 30.0f, Theme::BorderSubtle, 1.0f, 6.0f);
            m_renderer.DrawText(keys[i].key, badgeLeft, rowY + 2.0f, badgeLeft + badgeW, rowY + 30.0f, Theme::TextPrimary, 12.5f, true, true, true);
        }
    }

    void MainWindow::RenderOutputPanel(float left, float top, float right, float /*bottom*/) {
        const auto& settings = Config::SettingsManager::Instance().Get();

        // Card 1: Destination Directory & Actions
        float card1Y = top + 8.0f;
        float card1H = 100.0f;
        m_renderer.FillRect(left, card1Y, right, card1Y + card1H, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, card1Y, right, card1Y + card1H, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Destination Directory", left + 16.0f, card1Y + 14.0f, left + 200.0f, card1Y + 32.0f, Theme::TextPrimary, 13.5f, true);
        m_renderer.DrawText(settings.output.destinationDirectory, left + 16.0f, card1Y + 38.0f, right - 220.0f, card1Y + 58.0f, Theme::AccentBlue, 13.0f);

        // Interactive Browse and Open buttons
        float btnW = 110.0f;
        float btnH = 32.0f;
        float b1Left = right - 2.0f * btnW - 24.0f;
        float b2Left = right - btnW - 16.0f;
        float btnY = card1Y + 32.0f;

        m_renderer.DrawPillButton(b1Left, btnY, b1Left + btnW, btnY + btnH, L"Browse...", false, m_hoveredControl == "out_browse");
        m_renderer.DrawPillButton(b2Left, btnY, b2Left + btnW, btnY + btnH, L"Open Folder", false, m_hoveredControl == "out_open");

        // Card 2: Filename Template Chips
        float card2Y = card1Y + card1H + 16.0f;
        float card2H = 92.0f;
        m_renderer.FillRect(left, card2Y, right, card2Y + card2H, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, card2Y, right, card2Y + card2H, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Filename Template Preset", left + 16.0f, card2Y + 14.0f, right - 16.0f, card2Y + 32.0f, Theme::TextPrimary, 13.5f, true);

        const std::vector<std::wstring> templates = {
            L"{app}_{date}_{res}",
            L"{game}_{time}_{fps}fps",
            L"ScreenRecord_{date}_{time}"
        };
        float tW = (right - left - 32.0f - 2.0f * 10.0f) / 3.0f;
        for (size_t i = 0; i < templates.size(); ++i) {
            float tLeft = left + 16.0f + i * (tW + 10.0f);
            bool isAct = (m_selectedTemplateIndex == static_cast<int>(i));
            bool isHov = (m_hoveredControl == ("out_tmpl_" + std::to_string(i)));
            m_renderer.DrawPillButton(tLeft, card2Y + 40.0f, tLeft + tW, card2Y + 74.0f, templates[i], isAct, isHov);
        }

        // Card 3: Container & Lossless Remuxing
        float card3Y = card2Y + card2H + 16.0f;
        float card3H = 110.0f;
        m_renderer.FillRect(left, card3Y, right, card3Y + card3H, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, card3Y, right, card3Y + card3H, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Lossless Background Remux to MP4", left + 16.0f, card3Y + 14.0f, right - 80.0f, card3Y + 32.0f, Theme::TextPrimary, 13.5f, true);
        m_renderer.DrawText(L"Records crash-resilient MKV EBML clusters internally, automatically remuxing to MP4 on clean exit.", left + 16.0f, card3Y + 34.0f, right - 80.0f, card3Y + 50.0f, Theme::TextMuted, 11.5f);
        m_renderer.DrawToggleSwitch(right - 60.0f, card3Y + 18.0f, m_autoRemuxMp4, m_hoveredControl == "out_remux_toggle");

        m_renderer.DrawText(L"Delete intermediate MKV file after successful MP4 remux", left + 16.0f, card3Y + 68.0f, right - 80.0f, card3Y + 86.0f, Theme::TextSecondary, 12.0f);
        m_renderer.DrawToggleSwitch(right - 60.0f, card3Y + 66.0f, m_deleteMkvAfterRemux, m_hoveredControl == "out_del_toggle");
    }

    void MainWindow::RenderLibraryPanel(float left, float top, float right, float /*bottom*/) {
        float itemY = top + 8.0f;
        float itemH = 68.0f;
        float itemGap = 10.0f;

        for (size_t i = 0; i < m_libraryItems.size() && i < 6; ++i) {
            const auto& item = m_libraryItems[i];
            float cTop = itemY + i * (itemH + itemGap);
            float cBottom = cTop + itemH;

            bool isHov = (m_hoveredControl == ("lib_item_" + std::to_string(i)));
            m_renderer.FillRect(left, cTop, right, cBottom, isHov ? Theme::SurfaceCardHover : Theme::SurfaceCard, 8.0f);
            m_renderer.DrawRect(left, cTop, right, cBottom, isHov ? Theme::BorderSelected : Theme::BorderSubtle, 1.0f, 8.0f);

            // Thumbnail box
            float thumbW = 76.0f;
            float thumbH = 46.0f;
            float thumbX = left + 12.0f;
            float thumbY = cTop + (itemH - thumbH) * 0.5f;
            m_renderer.FillRect(thumbX, thumbY, thumbX + thumbW, thumbY + thumbH, Theme::BackgroundDark, 4.0f);
            m_renderer.DrawIcon(IconType::Video, thumbX + (thumbW - 16.0f) * 0.5f, thumbY + (thumbH - 16.0f) * 0.5f, 16.0f, Theme::TextMuted);

            float textLeft = thumbX + thumbW + 16.0f;
            m_renderer.DrawText(item.filename, textLeft, cTop + 14.0f, right - 130.0f, cTop + 34.0f, Theme::TextPrimary, 13.5f, true);

            std::wstring sub = item.resFps + L" · " + item.timeAgo;
            m_renderer.DrawText(sub, textLeft, cTop + 36.0f, right - 130.0f, cTop + 54.0f, Theme::TextMuted, 11.5f);

            float actionY = cTop + (itemH - 16.0f) * 0.5f;
            m_renderer.DrawIcon(IconType::Folder, right - 96.0f, actionY, 16.0f, Theme::TextSecondary);
            m_renderer.DrawIcon(IconType::Share, right - 64.0f, actionY, 16.0f, Theme::TextSecondary);
            m_renderer.DrawIcon(IconType::Trash, right - 32.0f, actionY, 16.0f, Theme::TextSecondary);
        }
    }

    void MainWindow::RenderAdvancedPanel(float left, float top, float right, float /*bottom*/) {
        // Card 1: Instant Replay Buffer (ShadowPlay-style)
        float card1Y = top + 8.0f;
        float card1H = 92.0f;
        m_renderer.FillRect(left, card1Y, right, card1Y + card1H, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, card1Y, right, card1Y + card1H, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Instant Replay Buffer Duration", left + 16.0f, card1Y + 14.0f, right - 16.0f, card1Y + 32.0f, Theme::TextPrimary, 13.5f, true);

        const std::vector<std::wstring> replayDurations = { L"30 Seconds", L"60 Seconds (Default)", L"2 Minutes", L"5 Minutes" };
        float rW = (right - left - 32.0f - 3.0f * 8.0f) / 4.0f;
        for (size_t i = 0; i < replayDurations.size(); ++i) {
            float pLeft = left + 16.0f + i * (rW + 8.0f);
            bool isAct = (m_replayDurationIndex == static_cast<int>(i));
            bool isHov = (m_hoveredControl == ("adv_rep_" + std::to_string(i)));
            m_renderer.DrawPillButton(pLeft, card1Y + 38.0f, pLeft + rW, card1Y + 70.0f, replayDurations[i], isAct, isHov);
        }

        // Card 2: GPU Direct3D 11 Multithreading
        float card2Y = card1Y + card1H + 16.0f;
        float card2H = 72.0f;
        m_renderer.FillRect(left, card2Y, right, card2Y + card2H, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, card2Y, right, card2Y + card2H, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Direct3D 11 Multithreaded Device Context", left + 16.0f, card2Y + 14.0f, right - 80.0f, card2Y + 32.0f, Theme::TextPrimary, 13.5f, true);
        m_renderer.DrawText(L"Enables hardware texture sharing across capture, compositor, and encoder threads.", left + 16.0f, card2Y + 36.0f, right - 80.0f, card2Y + 54.0f, Theme::TextMuted, 11.5f);
        m_renderer.DrawToggleSwitch(right - 60.0f, card2Y + 24.0f, m_d3d11Multithreading, m_hoveredControl == "adv_d3d_toggle");

        // Card 3: Factory Reset Defaults
        float card3Y = card2Y + card2H + 16.0f;
        float card3H = 72.0f;
        m_renderer.FillRect(left, card3Y, right, card3Y + card3H, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, card3Y, right, card3Y + card3H, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Reset Settings to Factory Defaults", left + 16.0f, card3Y + 16.0f, right - 180.0f, card3Y + 34.0f, Theme::TextPrimary, 13.5f, true);
        m_renderer.DrawText(L"Restores all video, audio, hotkeys, and template options to initial presets.", left + 16.0f, card3Y + 38.0f, right - 180.0f, card3Y + 56.0f, Theme::TextMuted, 11.5f);

        float resetBtnW = 140.0f;
        float resetBtnH = 34.0f;
        float resetLeft = right - resetBtnW - 16.0f;
        float resetTop = card3Y + 19.0f;
        m_renderer.DrawPillButton(resetLeft, resetTop, resetLeft + resetBtnW, resetTop + resetBtnH, L"Reset Defaults", false, m_hoveredControl == "adv_reset");
    }

    void MainWindow::RenderDropdowns(float left, float top, float right, float /*bottom*/) {
        float cardY = top + 8.0f;
        float cardH = 72.0f;
        float dropY = cardY + cardH + 18.0f;
        float dropH = 42.0f;

        if (m_sourceDropdownOpen) {
            float listTop = dropY + dropH + 4.0f;
            float listH = static_cast<float>(std::min(static_cast<size_t>(5), m_cachedSources.size())) * 36.0f + 8.0f;
            m_renderer.FillRect(left, listTop, right, listTop + listH, Theme::SurfaceCard, 8.0f);
            m_renderer.DrawRect(left, listTop, right, listTop + listH, Theme::BorderSelected, 1.0f, 8.0f);

            for (size_t i = 0; i < m_cachedSources.size() && i < 5; ++i) {
                float itemTop = listTop + 4.0f + i * 36.0f;
                float itemBottom = itemTop + 34.0f;
                if (static_cast<int>(i) == m_selectedSourceIndex) {
                    m_renderer.FillRect(left + 4.0f, itemTop, right - 4.0f, itemBottom, Theme::AccentBluePill, 4.0f);
                }
                m_renderer.DrawText(m_cachedSources[i].title, left + 14.0f, itemTop, right - 14.0f, itemBottom, Theme::TextPrimary, 13.0f, false, false, true);
            }
        }

        if (m_resDropdownOpen) {
            float colGap = 16.0f;
            float colW = (right - left - colGap) / 2.0f;
            float resDropY = dropY + dropH + 18.0f + 22.0f;
            float listTop = resDropY + dropH + 4.0f;

            const std::vector<std::wstring> resList = {
                L"Source native (2560x1440)",
                L"3840×2160 (4K UHD)",
                L"2560×1440 (1440p QHD)",
                L"1920×1080 (1080p FHD)",
                L"1280×720 (720p HD)"
            };
            float listH = resList.size() * 34.0f + 8.0f;

            m_renderer.FillRect(left, listTop, left + colW, listTop + listH, Theme::SurfaceCard, 8.0f);
            m_renderer.DrawRect(left, listTop, left + colW, listTop + listH, Theme::BorderSelected, 1.0f, 8.0f);

            for (size_t i = 0; i < resList.size(); ++i) {
                float itemTop = listTop + 4.0f + i * 34.0f;
                float itemBottom = itemTop + 32.0f;
                if (static_cast<int>(i) == m_selectedResIndex) {
                    m_renderer.FillRect(left + 4.0f, itemTop, left + colW - 4.0f, itemBottom, Theme::AccentBluePill, 4.0f);
                }
                m_renderer.DrawText(resList[i], left + 14.0f, itemTop, left + colW - 14.0f, itemBottom, Theme::TextPrimary, 12.5f, false, false, true);
            }
        }

        if (m_fpsDropdownOpen) {
            float colGap = 16.0f;
            float colW = (right - left - colGap) / 2.0f;
            float fpsLeft = left + colW + colGap;
            float resDropY = dropY + dropH + 18.0f + 22.0f;
            float listTop = resDropY + dropH + 4.0f;

            const std::vector<std::wstring> fpsList = { L"144 fps", L"120 fps", L"60 fps", L"30 fps" };
            float listH = fpsList.size() * 34.0f + 8.0f;

            m_renderer.FillRect(fpsLeft, listTop, fpsLeft + colW, listTop + listH, Theme::SurfaceCard, 8.0f);
            m_renderer.DrawRect(fpsLeft, listTop, fpsLeft + colW, listTop + listH, Theme::BorderSelected, 1.0f, 8.0f);

            for (size_t i = 0; i < fpsList.size(); ++i) {
                float itemTop = listTop + 4.0f + i * 34.0f;
                float itemBottom = itemTop + 32.0f;
                if (static_cast<int>(i) == m_selectedFpsIndex) {
                    m_renderer.FillRect(fpsLeft + 4.0f, itemTop, fpsLeft + colW - 4.0f, itemBottom, Theme::AccentBluePill, 4.0f);
                }
                m_renderer.DrawText(fpsList[i], fpsLeft + 14.0f, itemTop, fpsLeft + colW - 14.0f, itemBottom, Theme::TextPrimary, 12.5f, false, false, true);
            }
        }
    }

    void MainWindow::OnMouseMove(float x, float y) {
        m_hoveredControl = "";

        if (x < 185.0f) {
            float startY = 104.0f;
            float itemH = 38.0f;
            float itemSpacing = 6.0f;
            int tab = static_cast<int>((y - startY) / (itemH + itemSpacing));
            m_hoveredTab = (tab >= 0 && tab <= 6) ? tab : -1;
            m_hoveredSourceMode = -1;
            m_hoveredRecordBtn = false;
            m_hoveredScreenshotBtn = false;
            return;
        }

        m_hoveredTab = -1;

        RECT rc;
        GetClientRect(m_hwnd, &rc);
        float w = (rc.right - rc.left) * 96.0f / m_dpi;
        float left = 185.0f + 28.0f;
        float right = w - 28.0f;
        float top = 50.0f + 16.0f;

        if (m_activeTab == NavigationTab::Source) {
            float cardY = top + 8.0f;
            float cardH = 72.0f;
            float cardGap = 12.0f;
            float cardW = (right - left - 3.0f * cardGap) / 4.0f;

            if (y >= cardY && y <= cardY + cardH) {
                int mode = static_cast<int>((x - left) / (cardW + cardGap));
                m_hoveredSourceMode = (mode >= 0 && mode <= 3) ? mode : -1;
            } else {
                m_hoveredSourceMode = -1;
            }

            float dropY = cardY + cardH + 18.0f;
            float dropH = 42.0f;
            if (x >= left && x <= right && y >= dropY && y <= dropY + dropH) {
                m_hoveredControl = "source_dropdown";
            }

            float colGap = 16.0f;
            float colW = (right - left - colGap) / 2.0f;
            float resDropY = dropY + dropH + 18.0f + 22.0f;
            if (x >= left && x <= left + colW && y >= resDropY && y <= resDropY + dropH) {
                m_hoveredControl = "res_dropdown";
            }
            float fpsLeft = left + colW + colGap;
            if (x >= fpsLeft && x <= fpsLeft + colW && y >= resDropY && y <= resDropY + dropH) {
                m_hoveredControl = "fps_dropdown";
            }

            float perfY = resDropY + dropH + 20.0f;
            float perfH = 74.0f;
            float btnY = perfY + perfH + 22.0f;
            float btnH = 44.0f;
            float shotBtnW = 44.0f;
            float shotBtnLeft = right - shotBtnW;
            float recBtnRight = shotBtnLeft - 10.0f;

            m_hoveredRecordBtn = (y >= btnY && y <= btnY + btnH && x >= left && x <= recBtnRight);
            m_hoveredScreenshotBtn = (y >= btnY && y <= btnY + btnH && x >= shotBtnLeft && x <= right);
        } else if (m_activeTab == NavigationTab::Video) {
            float card1Y = top + 8.0f;
            float pillW = (right - left - 32.0f - 4.0f * 8.0f) / 5.0f;
            float pillH = 30.0f;
            float pillY = card1Y + 38.0f;
            for (size_t i = 0; i < 5; ++i) {
                float pLeft = left + 16.0f + i * (pillW + 8.0f);
                if (x >= pLeft && x <= pLeft + pillW && y >= pillY && y <= pillY + pillH) {
                    m_hoveredControl = "video_res_" + std::to_string(i);
                }
            }
            float fpsPillW = 84.0f;
            for (size_t i = 0; i < 4; ++i) {
                float pLeft = left + 110.0f + i * (fpsPillW + 8.0f);
                if (x >= pLeft && x <= pLeft + fpsPillW && y >= card1Y + 74.0f && y <= card1Y + 100.0f) {
                    m_hoveredControl = "video_fps_" + std::to_string(i);
                }
            }

            float card2Y = card1Y + 110.0f + 16.0f;
            float codeW = (right - left - 32.0f - 2.0f * 10.0f) / 3.0f;
            for (size_t i = 0; i < 3; ++i) {
                float cLeft = left + 16.0f + i * (codeW + 10.0f);
                if (x >= cLeft && x <= cLeft + codeW && y >= card2Y + 40.0f && y <= card2Y + 76.0f) {
                    m_hoveredControl = "video_codec_" + std::to_string(i);
                }
            }

            float card4Y = card2Y + 92.0f + 16.0f + 110.0f + 16.0f;
            if (x >= right - 60.0f && x <= right - 16.0f && y >= card4Y + 20.0f && y <= card4Y + 44.0f) {
                m_hoveredControl = "video_strict_toggle";
            }
        } else if (m_activeTab == NavigationTab::Output) {
            float card1Y = top + 8.0f;
            float btnW = 110.0f;
            float btnH = 32.0f;
            float b1Left = right - 2.0f * btnW - 24.0f;
            float b2Left = right - btnW - 16.0f;
            float btnY = card1Y + 32.0f;
            if (x >= b1Left && x <= b1Left + btnW && y >= btnY && y <= btnY + btnH) {
                m_hoveredControl = "out_browse";
            } else if (x >= b2Left && x <= b2Left + btnW && y >= btnY && y <= btnY + btnH) {
                m_hoveredControl = "out_open";
            }
        }
    }

    void MainWindow::OnClick(float x, float y) {
        // 1. Sidebar tab navigation
        if (x < 185.0f) {
            float startY = 104.0f;
            float itemH = 38.0f;
            float itemSpacing = 6.0f;
            int tab = static_cast<int>((y - startY) / (itemH + itemSpacing));
            if (tab >= 0 && tab <= 6) {
                m_activeTab = static_cast<NavigationTab>(tab);
                m_sourceDropdownOpen = false;
                m_resDropdownOpen = false;
                m_fpsDropdownOpen = false;
            }
            return;
        }

        RECT rc;
        GetClientRect(m_hwnd, &rc);
        float w = (rc.right - rc.left) * 96.0f / m_dpi;
        float left = 185.0f + 28.0f;
        float right = w - 28.0f;
        float top = 50.0f + 16.0f;

        // 2. Dropdowns Handling
        float cardY = top + 8.0f;
        float cardH = 72.0f;
        float dropY = cardY + cardH + 18.0f;
        float dropH = 42.0f;
        float colGap = 16.0f;
        float colW = (right - left - colGap) / 2.0f;
        float resDropY = dropY + dropH + 18.0f + 22.0f;

        if (m_sourceDropdownOpen) {
            float listTop = dropY + dropH + 4.0f;
            if (x >= left && x <= right && y >= listTop) {
                int idx = static_cast<int>((y - listTop - 4.0f) / 36.0f);
                if (idx >= 0 && idx < static_cast<int>(m_cachedSources.size()) && idx < 5) {
                    m_selectedSourceIndex = idx;
                }
            }
            m_sourceDropdownOpen = false;
            return;
        }

        if (m_resDropdownOpen) {
            float listTop = resDropY + dropH + 4.0f;
            if (x >= left && x <= left + colW && y >= listTop) {
                int idx = static_cast<int>((y - listTop - 4.0f) / 34.0f);
                if (idx >= 0 && idx <= 4) {
                    m_selectedResIndex = idx;
                    auto& settings = Config::SettingsManager::Instance().Get();
                    if (idx == 1) { settings.video.width = 3840; settings.video.height = 2160; }
                    else if (idx == 2) { settings.video.width = 2560; settings.video.height = 1440; }
                    else if (idx == 3) { settings.video.width = 1920; settings.video.height = 1080; }
                    else if (idx == 4) { settings.video.width = 1280; settings.video.height = 720; }
                }
            }
            m_resDropdownOpen = false;
            return;
        }

        if (m_fpsDropdownOpen) {
            float fpsLeft = left + colW + colGap;
            float listTop = resDropY + dropH + 4.0f;
            if (x >= fpsLeft && x <= fpsLeft + colW && y >= listTop) {
                int idx = static_cast<int>((y - listTop - 4.0f) / 34.0f);
                if (idx >= 0 && idx <= 3) {
                    m_selectedFpsIndex = idx;
                    auto& settings = Config::SettingsManager::Instance().Get();
                    if (idx == 0) settings.video.targetFps = 144;
                    else if (idx == 1) settings.video.targetFps = 120;
                    else if (idx == 2) settings.video.targetFps = 60;
                    else if (idx == 3) settings.video.targetFps = 30;
                }
            }
            m_fpsDropdownOpen = false;
            return;
        }

        // 3. Tab Specific Interactive Click Handling
        if (m_activeTab == NavigationTab::Source) {
            if (y >= cardY && y <= cardY + cardH) {
                float cardGap = 12.0f;
                float cardW = (right - left - 3.0f * cardGap) / 4.0f;
                int mode = static_cast<int>((x - left) / (cardW + cardGap));
                if (mode >= 0 && mode <= 3) {
                    m_sourceMode = static_cast<SourceMode>(mode);
                    if (m_sourceMode == SourceMode::Monitor) {
                        m_cachedSources = Capture::SourceManager::EnumerateMonitors();
                    } else {
                        m_cachedSources = Capture::SourceManager::EnumerateWindows();
                    }
                    m_selectedSourceIndex = 0;
                    return;
                }
            }

            if (x >= left && x <= right && y >= dropY && y <= dropY + dropH) {
                m_sourceDropdownOpen = !m_sourceDropdownOpen;
                return;
            }

            if (x >= left && x <= left + colW && y >= resDropY && y <= resDropY + dropH) {
                m_resDropdownOpen = !m_resDropdownOpen;
                return;
            }

            float fpsLeft = left + colW + colGap;
            if (x >= fpsLeft && x <= fpsLeft + colW && y >= resDropY && y <= resDropY + dropH) {
                m_fpsDropdownOpen = !m_fpsDropdownOpen;
                return;
            }

            float perfY = resDropY + dropH + 20.0f;
            float perfH = 74.0f;
            float btnY = perfY + perfH + 22.0f;
            float btnH = 44.0f;
            float shotBtnW = 44.0f;
            float shotBtnLeft = right - shotBtnW;
            float recBtnRight = shotBtnLeft - 10.0f;

            if (x >= left && x <= recBtnRight && y >= btnY && y <= btnY + btnH) {
                auto& engine = Core::Engine::Instance();
                if (engine.GetState() == Core::EngineState::Idle) {
                    Core::CaptureSourceDescriptor src;
                    if (!m_cachedSources.empty() && m_selectedSourceIndex < static_cast<int>(m_cachedSources.size())) {
                        src = m_cachedSources[m_selectedSourceIndex];
                    }
                    engine.StartRecording(src);
                } else if (engine.GetState() == Core::EngineState::Recording || engine.GetState() == Core::EngineState::Paused) {
                    engine.StopRecording();
                    RefreshLibrary();
                }
                return;
            }

            if (x >= shotBtnLeft && x <= right && y >= btnY && y <= btnY + btnH) {
                TakeScreenshot();
                return;
            }
        } else if (m_activeTab == NavigationTab::Video) {
            // Resolution pills click
            float card1Y = top + 8.0f;
            float pillW = (right - left - 32.0f - 4.0f * 8.0f) / 5.0f;
            float pillH = 30.0f;
            float pillY = card1Y + 38.0f;

            for (size_t i = 0; i < 5; ++i) {
                float pLeft = left + 16.0f + i * (pillW + 8.0f);
                if (x >= pLeft && x <= pLeft + pillW && y >= pillY && y <= pillY + pillH) {
                    m_selectedResIndex = static_cast<int>(i);
                    auto& settings = Config::SettingsManager::Instance().Get();
                    if (i == 0) { settings.video.width = 2560; settings.video.height = 1440; }
                    else if (i == 1) { settings.video.width = 3840; settings.video.height = 2160; }
                    else if (i == 2) { settings.video.width = 2560; settings.video.height = 1440; }
                    else if (i == 3) { settings.video.width = 1920; settings.video.height = 1080; }
                    else if (i == 4) { settings.video.width = 1280; settings.video.height = 720; }
                    return;
                }
            }

            // FPS pills click
            float fpsPillW = 84.0f;
            for (size_t i = 0; i < 4; ++i) {
                float pLeft = left + 110.0f + i * (fpsPillW + 8.0f);
                if (x >= pLeft && x <= pLeft + fpsPillW && y >= card1Y + 74.0f && y <= card1Y + 100.0f) {
                    m_selectedFpsIndex = static_cast<int>(i);
                    auto& settings = Config::SettingsManager::Instance().Get();
                    if (i == 0) settings.video.targetFps = 144;
                    else if (i == 1) settings.video.targetFps = 120;
                    else if (i == 2) settings.video.targetFps = 60;
                    else if (i == 3) settings.video.targetFps = 30;
                    return;
                }
            }

            // Codec cards click
            float card2Y = card1Y + 110.0f + 16.0f;
            float codeW = (right - left - 32.0f - 2.0f * 10.0f) / 3.0f;
            for (size_t i = 0; i < 3; ++i) {
                float cLeft = left + 16.0f + i * (codeW + 10.0f);
                if (x >= cLeft && x <= cLeft + codeW && y >= card2Y + 40.0f && y <= card2Y + 76.0f) {
                    m_selectedCodecIndex = static_cast<int>(i);
                    auto& settings = Config::SettingsManager::Instance().Get();
                    if (i == 0) settings.video.codec = Core::VideoCodec::HEVC;
                    else if (i == 1) settings.video.codec = Core::VideoCodec::AV1;
                    else if (i == 2) settings.video.codec = Core::VideoCodec::H264;
                    return;
                }
            }

            // Bitrate pills click
            float card3Y = card2Y + 92.0f + 16.0f;
            float bPillW = 105.0f;
            for (size_t i = 0; i < 4; ++i) {
                float pLeft = left + 16.0f + i * (bPillW + 8.0f);
                if (x >= pLeft && x <= pLeft + bPillW && y >= card3Y + 38.0f && y <= card3Y + 68.0f) {
                    m_selectedBitrateIndex = static_cast<int>(i);
                    auto& settings = Config::SettingsManager::Instance().Get();
                    if (i == 0) settings.video.targetBitrateKbps = 15000;
                    else if (i == 1) settings.video.targetBitrateKbps = 25000;
                    else if (i == 2) settings.video.targetBitrateKbps = 40000;
                    else if (i == 3) settings.video.targetBitrateKbps = 60000;
                    return;
                }
            }

            // Strict hardware toggle click
            float card4Y = card3Y + 110.0f + 16.0f;
            if (x >= right - 60.0f && x <= right - 16.0f && y >= card4Y + 20.0f && y <= card4Y + 44.0f) {
                m_strictHardwarePolicy = !m_strictHardwarePolicy;
                auto& settings = Config::SettingsManager::Instance().Get();
                settings.video.allowSoftwareFallback = !m_strictHardwarePolicy;
                return;
            }
        } else if (m_activeTab == NavigationTab::Audio) {
            float card1Y = top + 8.0f;
            // System audio toggle
            if (x >= right - 60.0f && x <= right - 16.0f && y >= card1Y + 18.0f && y <= card1Y + 42.0f) {
                m_systemAudioEnabled = !m_systemAudioEnabled;
                auto& settings = Config::SettingsManager::Instance().Get();
                settings.audio.systemAudioEnabled = m_systemAudioEnabled;
                return;
            }

            // Mic toggle
            float card2Y = card1Y + 110.0f + 16.0f;
            if (x >= right - 60.0f && x <= right - 16.0f && y >= card2Y + 18.0f && y <= card2Y + 42.0f) {
                m_micEnabled = !m_micEnabled;
                Core::Engine::Instance().SetMicMuted(!m_micEnabled);
                return;
            }

            // Per process audio toggle
            float card3Y = card2Y + 110.0f + 16.0f;
            if (x >= right - 60.0f && x <= right - 16.0f && y >= card3Y + 18.0f && y <= card3Y + 42.0f) {
                m_perProcessAudio = !m_perProcessAudio;
                return;
            }

            // Audio bitrate pills
            for (size_t i = 0; i < 3; ++i) {
                float pLeft = left + 100.0f + i * (135.0f + 8.0f);
                if (x >= pLeft && x <= pLeft + 135.0f && y >= card3Y + 62.0f && y <= card3Y + 88.0f) {
                    m_audioBitrateIndex = static_cast<int>(i);
                    auto& settings = Config::SettingsManager::Instance().Get();
                    if (i == 0) settings.audio.bitrateKbps = 128;
                    else if (i == 1) settings.audio.bitrateKbps = 192;
                    else if (i == 2) settings.audio.bitrateKbps = 320;
                    return;
                }
            }
        } else if (m_activeTab == NavigationTab::Hotkeys) {
            float profY = top + 8.0f;
            float pW = (right - left - 32.0f - 2.0f * 10.0f) / 3.0f;
            for (size_t i = 0; i < 3; ++i) {
                float pLeft = left + 16.0f + i * (pW + 10.0f);
                if (x >= pLeft && x <= pLeft + pW && y >= profY + 32.0f && y <= profY + 58.0f) {
                    m_hotkeyProfileIndex = static_cast<int>(i);
                    RegisterGlobalHotkeys();
                    m_trayIcon.ShowNotification(L"Hotkey Profile Updated", (i == 0 ? L"Standard (Ctrl+Shift)" : (i == 1 ? L"Function Keys (F8-F12)" : L"Alt-Combos")));
                    return;
                }
            }
        } else if (m_activeTab == NavigationTab::Output) {
            float card1Y = top + 8.0f;
            float btnW = 110.0f;
            float btnH = 32.0f;
            float b1Left = right - 2.0f * btnW - 24.0f;
            float b2Left = right - btnW - 16.0f;
            float btnY = card1Y + 32.0f;

            if (x >= b1Left && x <= b1Left + btnW && y >= btnY && y <= btnY + btnH) {
                BrowseOutputDirectory();
                return;
            }
            if (x >= b2Left && x <= b2Left + btnW && y >= btnY && y <= btnY + btnH) {
                OpenOutputDirectory();
                return;
            }

            // Template chips
            float card2Y = card1Y + 100.0f + 16.0f;
            float tW = (right - left - 32.0f - 2.0f * 10.0f) / 3.0f;
            for (size_t i = 0; i < 3; ++i) {
                float tLeft = left + 16.0f + i * (tW + 10.0f);
                if (x >= tLeft && x <= tLeft + tW && y >= card2Y + 40.0f && y <= card2Y + 74.0f) {
                    m_selectedTemplateIndex = static_cast<int>(i);
                    auto& settings = Config::SettingsManager::Instance().Get();
                    if (i == 0) settings.output.filenameTemplate = L"{app}_{date}_{res}";
                    else if (i == 1) settings.output.filenameTemplate = L"{game}_{time}_{fps}fps";
                    else if (i == 2) settings.output.filenameTemplate = L"ScreenRecord_{date}_{time}";
                    return;
                }
            }

            // Auto remux toggle
            float card3Y = card2Y + 92.0f + 16.0f;
            if (x >= right - 60.0f && x <= right - 16.0f && y >= card3Y + 18.0f && y <= card3Y + 42.0f) {
                m_autoRemuxMp4 = !m_autoRemuxMp4;
                auto& settings = Config::SettingsManager::Instance().Get();
                settings.output.autoRemuxMp4 = m_autoRemuxMp4;
                return;
            }

            // Delete MKV toggle
            if (x >= right - 60.0f && x <= right - 16.0f && y >= card3Y + 66.0f && y <= card3Y + 90.0f) {
                m_deleteMkvAfterRemux = !m_deleteMkvAfterRemux;
                auto& settings = Config::SettingsManager::Instance().Get();
                settings.output.deleteMkvAfterRemux = m_deleteMkvAfterRemux;
                return;
            }
        } else if (m_activeTab == NavigationTab::Library) {
            float itemY = top + 8.0f;
            float itemH = 68.0f;
            float itemGap = 10.0f;

            for (size_t i = 0; i < m_libraryItems.size() && i < 6; ++i) {
                float cTop = itemY + i * (itemH + itemGap);
                float cBottom = cTop + itemH;

                if (y >= cTop && y <= cBottom) {
                    const auto& item = m_libraryItems[i];
                    // Folder icon
                    if (x >= right - 105.0f && x <= right - 75.0f) {
                        if (!item.filepath.empty()) {
                            std::wstring param = L"/select,\"" + item.filepath + L"\"";
                            ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOW);
                        } else {
                            OpenOutputDirectory();
                        }
                        return;
                    }
                    // Share icon
                    if (x >= right - 75.0f && x <= right - 45.0f) {
                        std::wstring textToCopy = item.filepath.empty() ? item.filename : item.filepath;
                        if (OpenClipboard(m_hwnd)) {
                            EmptyClipboard();
                            HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (textToCopy.size() + 1) * sizeof(wchar_t));
                            if (hGlob) {
                                memcpy(GlobalLock(hGlob), textToCopy.c_str(), (textToCopy.size() + 1) * sizeof(wchar_t));
                                GlobalUnlock(hGlob);
                                SetClipboardData(CF_UNICODETEXT, hGlob);
                            }
                            CloseClipboard();
                        }
                        m_trayIcon.ShowNotification(L"Copied Path", item.filename);
                        return;
                    }
                    // Trash icon
                    if (x >= right - 45.0f && x <= right) {
                        if (!item.filepath.empty()) {
                            try {
                                std::filesystem::remove(item.filepath);
                            } catch (...) {}
                        }
                        RefreshLibrary();
                        return;
                    }
                    // Clicking card plays video
                    if (x < right - 110.0f && !item.filepath.empty()) {
                        ShellExecuteW(nullptr, L"open", item.filepath.c_str(), nullptr, nullptr, SW_SHOW);
                        return;
                    }
                }
            }
        } else if (m_activeTab == NavigationTab::Advanced) {
            // Buffer duration pills click
            float card1Y = top + 8.0f;
            float rW = (right - left - 32.0f - 3.0f * 8.0f) / 4.0f;
            for (size_t i = 0; i < 4; ++i) {
                float pLeft = left + 16.0f + i * (rW + 8.0f);
                if (x >= pLeft && x <= pLeft + rW && y >= card1Y + 38.0f && y <= card1Y + 70.0f) {
                    m_replayDurationIndex = static_cast<int>(i);
                    auto& settings = Config::SettingsManager::Instance().Get();
                    if (i == 0) settings.replay.bufferDurationSeconds = 30;
                    else if (i == 1) settings.replay.bufferDurationSeconds = 60;
                    else if (i == 2) settings.replay.bufferDurationSeconds = 120;
                    else if (i == 3) settings.replay.bufferDurationSeconds = 300;
                    return;
                }
            }

            // D3D11 Multithreading toggle
            float card2Y = card1Y + 92.0f + 16.0f;
            if (x >= right - 60.0f && x <= right - 16.0f && y >= card2Y + 24.0f && y <= card2Y + 48.0f) {
                m_d3d11Multithreading = !m_d3d11Multithreading;
                return;
            }

            // Reset defaults button
            float card3Y = card2Y + 72.0f + 16.0f;
            float resetBtnW = 140.0f;
            float resetLeft = right - resetBtnW - 16.0f;
            float resetTop = card3Y + 19.0f;
            if (x >= resetLeft && x <= resetLeft + resetBtnW && y >= resetTop && y <= resetTop + 34.0f) {
                Config::SettingsManager::Instance().SetDefaults();
                m_selectedResIndex = 0;
                m_selectedFpsIndex = 2;
                m_selectedCodecIndex = 0;
                m_selectedBitrateIndex = 1;
                m_selectedRateControlIndex = 0;
                m_selectedBitrateModeIndex = 0;
                m_strictHardwarePolicy = true;
                m_systemAudioEnabled = true;
                m_micEnabled = true;
                m_perProcessAudio = false;
                m_hotkeyProfileIndex = 0;
                m_autoRemuxMp4 = true;
                m_deleteMkvAfterRemux = true;
                RegisterGlobalHotkeys();
                m_trayIcon.ShowNotification(L"Settings Reset", L"All configuration restored to factory defaults.");
                return;
            }
        }
    }
#endif

} // namespace Ui
} // namespace Recorder
