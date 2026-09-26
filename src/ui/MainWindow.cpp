#include "ui/MainWindow.h"
#include "core/Engine.h"
#include "config/Settings.h"
#include "config/PresetManager.h"
#include "capture/SourceManager.h"
#include <iostream>

#if defined(_WIN32)
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace Recorder::Ui {

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
            case WM_SIZE: {
                UINT width = LOWORD(lParam);
                UINT height = HIWORD(lParam);
                m_renderer.Resize(width, height);
                Render();
                return 0;
            }
            case WM_LBUTTONDOWN: {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                OnClick(x, y);
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
                    }
                } else if (hotkeyId == 102) { // Pause/Resume
                    if (engine.GetState() == Core::EngineState::Recording) {
                        engine.PauseRecording();
                    } else if (engine.GetState() == Core::EngineState::Paused) {
                        engine.ResumeRecording();
                    }
                } else if (hotkeyId == 103) { // Save Replay
                    engine.SaveReplay();
                } else if (hotkeyId == 104) { // Mic Mute Toggle
                    engine.SetMicMuted(!engine.IsMicMuted());
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
        RegisterHotKey(m_hwnd, 101, MOD_CONTROL | MOD_SHIFT, 'R'); // Start/Stop
        RegisterHotKey(m_hwnd, 102, MOD_CONTROL | MOD_SHIFT, 'P'); // Pause/Resume
        RegisterHotKey(m_hwnd, 103, MOD_CONTROL | MOD_SHIFT, 'S'); // Save Replay
        RegisterHotKey(m_hwnd, 104, MOD_CONTROL | MOD_SHIFT, 'M'); // Mute Mic
    }

    void MainWindow::UnregisterGlobalHotkeys() {
        if (!m_hwnd) return;
        UnregisterHotKey(m_hwnd, 101);
        UnregisterHotKey(m_hwnd, 102);
        UnregisterHotKey(m_hwnd, 103);
        UnregisterHotKey(m_hwnd, 104);
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
        wc.hbrBackground = CreateSolidBrush(RGB(20, 20, 23));
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(
            WS_EX_APPWINDOW,
            wc.lpszClassName,
            L"High-Performance Screen Recorder",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, width, height,
            nullptr, nullptr, wc.hInstance, this
        );

        if (!m_hwnd) return false;

        // Apply dark mode titlebar (Windows 10/11 DWM attribute)
        BOOL useDarkMode = TRUE;
        DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

        m_renderer.Initialize(m_hwnd);
        m_trayIcon.Initialize(m_hwnd, WM_USER + 101);

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
        m_trayIcon.SetOnStopRecord([]() {
            Core::Engine::Instance().StopRecording();
        });
        m_trayIcon.SetOnSaveReplay([]() {
            Core::Engine::Instance().SaveReplay();
        });
        m_trayIcon.SetOnExitApp([this]() {
            DestroyWindow(m_hwnd);
        });

        RegisterGlobalHotkeys();

        // Enumerate initial sources
        m_cachedSources = Capture::SourceManager::EnumerateMonitors();
        auto windows = Capture::SourceManager::EnumerateWindows();
        m_cachedSources.insert(m_cachedSources.end(), windows.begin(), windows.end());

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

#if defined(_WIN32)
    void MainWindow::Render() {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        float w = static_cast<float>(rc.right - rc.left);
        float h = static_cast<float>(rc.bottom - rc.top);

        m_renderer.BeginDraw();
        m_renderer.Clear(Theme::BackgroundDark);

        float sidebarWidth = 200.0f;
        float headerHeight = 70.0f;

        RenderSidebar(sidebarWidth, h);
        RenderHeader(sidebarWidth, 0.0f, w, headerHeight);
        RenderTabContent(sidebarWidth + 24.0f, headerHeight + 20.0f, w - 24.0f, h - 20.0f);

        m_renderer.EndDraw();
    }

    void MainWindow::RenderSidebar(float width, float height) {
        // Sidebar background
        m_renderer.FillRect(0, 0, width, height, Theme::SurfaceCard);
        m_renderer.DrawRect(width - 1.0f, 0, width, height, Theme::BorderSubtle);

        // App Logo & Title
        m_renderer.DrawText(L"PRO RECORDER", 20.0f, 24.0f, width - 20.0f, 50.0f, Theme::TextPrimary, 16.0f, true);

        // Navigation Tabs
        struct TabItem {
            NavigationTab tab;
            std::wstring label;
        };

        std::vector<TabItem> tabs = {
            { NavigationTab::Sources, L"Sources" },
            { NavigationTab::Video, L"Video & GPU" },
            { NavigationTab::Audio, L"Audio Mix" },
            { NavigationTab::Hotkeys, L"Hotkeys" },
            { NavigationTab::Output, L"Output Paths" },
            { NavigationTab::Library, L"Library" },
            { NavigationTab::Diagnostics, L"Diagnostics" }
        };

        float startY = 80.0f;
        float itemHeight = 42.0f;

        for (size_t i = 0; i < tabs.size(); ++i) {
            float top = startY + i * (itemHeight + 6.0f);
            float bottom = top + itemHeight;
            bool active = (m_activeTab == tabs[i].tab);

            if (active) {
                m_renderer.FillRect(12.0f, top, width - 12.0f, bottom, Theme::SurfaceCardHover, 6.0f);
                m_renderer.FillRect(12.0f, top + 8.0f, 16.0f, bottom - 8.0f, Theme::AccentBlue, 2.0f);
                m_renderer.DrawText(tabs[i].label, 28.0f, top + 11.0f, width - 20.0f, bottom, Theme::TextPrimary, 14.0f, true);
            } else {
                m_renderer.DrawText(tabs[i].label, 28.0f, top + 11.0f, width - 20.0f, bottom, Theme::TextSecondary, 14.0f, false);
            }
        }
    }

    void MainWindow::RenderHeader(float left, float top, float right, float height) {
        m_renderer.FillRect(left, top, right, height, Theme::SurfaceCard);
        m_renderer.DrawRect(left, height - 1.0f, right, height, Theme::BorderSubtle);

        auto state = Core::Engine::Instance().GetState();
        auto telem = Core::Engine::Instance().GetTelemetry();

        // Status pill
        std::wstring statusText = L"Status: " + std::wstring(Core::ToWString(state));
        ColorRGB statusColor = (state == Core::EngineState::Recording) ? Theme::RecordRed : Theme::TextSecondary;
        m_renderer.DrawText(statusText, left + 24.0f, top + 24.0f, left + 180.0f, top + 46.0f, statusColor, 14.0f, true);

        // Telemetry counters
        wchar_t telemBuf[128];
        swprintf_s(telemBuf, L"CPU: %.1f%%  |  Bitrate: %.0f kbps  |  Dropped: %llu",
                   telem.recorderCpuPercent, telem.currentBitrateKbps, telem.droppedFrames);
        m_renderer.DrawText(telemBuf, left + 180.0f, top + 26.0f, right - 220.0f, top + 46.0f, Theme::TextMuted, 12.0f);

        // Big Record Button
        float btnRight = right - 24.0f;
        float btnLeft = btnRight - 150.0f;
        float btnTop = top + 14.0f;
        float btnBottom = top + 56.0f;

        if (state == Core::EngineState::Recording) {
            m_renderer.FillRect(btnLeft, btnTop, btnRight, btnBottom, Theme::SurfaceCardHover, 6.0f);
            m_renderer.DrawRect(btnLeft, btnTop, btnRight, btnBottom, Theme::RecordRed, 2.0f, 6.0f);
            m_renderer.DrawText(L"Stop Recording", btnLeft, btnTop + 11.0f, btnRight, btnBottom, Theme::RecordRed, 14.0f, true, true);
        } else {
            m_renderer.FillRect(btnLeft, btnTop, btnRight, btnBottom, Theme::RecordRed, 6.0f);
            m_renderer.DrawText(L"Start Recording", btnLeft, btnTop + 11.0f, btnRight, btnBottom, Theme::TextPrimary, 14.0f, true, true);
        }
    }

    void MainWindow::RenderTabContent(float left, float top, float right, float bottom) {
        switch (m_activeTab) {
            case NavigationTab::Sources:     RenderSourcesPanel(left, top, right, bottom); break;
            case NavigationTab::Video:       RenderVideoPanel(left, top, right, bottom); break;
            case NavigationTab::Audio:       RenderAudioPanel(left, top, right, bottom); break;
            case NavigationTab::Hotkeys:     RenderHotkeysPanel(left, top, right, bottom); break;
            case NavigationTab::Output:      RenderOutputPanel(left, top, right, bottom); break;
            case NavigationTab::Library:     RenderLibraryPanel(left, top, right, bottom); break;
            case NavigationTab::Diagnostics: RenderDiagnosticsPanel(left, top, right, bottom); break;
        }
    }

    void MainWindow::RenderSourcesPanel(float left, float top, float right, float /*bottom*/) {
        m_renderer.DrawText(L"Capture Source Selection", left, top, right, top + 30.0f, Theme::TextPrimary, 18.0f, true);
        m_renderer.DrawText(L"Choose a display monitor or specific application window to capture via GPU-accelerated WGC.",
                            left, top + 32.0f, right, top + 54.0f, Theme::TextSecondary, 13.0f);

        float cardY = top + 70.0f;
        float cardWidth = 320.0f;
        float cardHeight = 70.0f;

        for (size_t i = 0; i < m_cachedSources.size() && i < 6; ++i) {
            float cLeft = left + (i % 2) * (cardWidth + 16.0f);
            float cTop = cardY + (i / 2) * (cardHeight + 12.0f);
            float cRight = cLeft + cardWidth;
            float cBottom = cTop + cardHeight;

            bool isSelected = (static_cast<int>(i) == m_selectedSourceIndex);
            ColorRGB bg = isSelected ? Theme::SurfaceCardHover : Theme::SurfaceCard;
            m_renderer.FillRect(cLeft, cTop, cRight, cBottom, bg, 8.0f);
            m_renderer.DrawRect(cLeft, cTop, cRight, cBottom, isSelected ? Theme::AccentBlue : Theme::BorderSubtle, isSelected ? 2.0f : 1.0f, 8.0f);

            std::wstring srcType = (m_cachedSources[i].type == Core::CaptureSourceType::Monitor) ? L"Monitor" : L"Window";
            m_renderer.DrawText(srcType, cLeft + 16.0f, cTop + 12.0f, cRight - 16.0f, cTop + 30.0f, Theme::AccentBlue, 11.0f, true);
            m_renderer.DrawText(m_cachedSources[i].title, cLeft + 16.0f, cTop + 30.0f, cRight - 16.0f, cBottom - 10.0f, Theme::TextPrimary, 13.0f, false);
        }
    }

    void MainWindow::RenderVideoPanel(float left, float top, float right, float /*bottom*/) {
        m_renderer.DrawText(L"Video & GPU Encoding", left, top, right, top + 30.0f, Theme::TextPrimary, 18.0f, true);
        
        const auto& settings = Config::SettingsManager::Instance().Get();

        float cardTop = top + 50.0f;
        m_renderer.FillRect(left, cardTop, right, cardTop + 240.0f, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, cardTop, right, cardTop + 240.0f, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Active Video Settings:", left + 20.0f, cardTop + 18.0f, right - 20.0f, cardTop + 40.0f, Theme::TextPrimary, 14.0f, true);
        
        wchar_t buf[256];
        swprintf_s(buf, L"• Resolution: %ux%u\n• Frame Rate: %u FPS (%s)\n• Bitrate: %u kbps (%s)\n• Codec: %s\n• Strict HW Policy: Enabled (No CPU software fallback)",
                   settings.video.width, settings.video.height, settings.video.targetFps,
                   (settings.video.rateControlMode == Core::RateControlMode::CFR ? L"CFR" : L"VFR"),
                   settings.video.targetBitrateKbps,
                   (settings.video.bitrateMode == Core::BitrateMode::CBR ? L"CBR" : (settings.video.bitrateMode == Core::BitrateMode::VBR ? L"VBR" : L"CQP")),
                   (settings.video.codec == Core::VideoCodec::HEVC ? L"HEVC / H.265" : (settings.video.codec == Core::VideoCodec::AV1 ? L"AV1" : L"H.264")));
        
        m_renderer.DrawText(buf, left + 20.0f, cardTop + 48.0f, right - 20.0f, cardTop + 200.0f, Theme::TextSecondary, 13.0f);
    }

    void MainWindow::RenderAudioPanel(float left, float top, float right, float /*bottom*/) {
        m_renderer.DrawText(L"Audio Mixer & Monitoring", left, top, right, top + 30.0f, Theme::TextPrimary, 18.0f, true);

        auto& engine = Core::Engine::Instance();
        float sysVu = 0.4f;
        float micVu = engine.IsMicMuted() ? 0.0f : 0.25f;

        float cardTop = top + 50.0f;
        m_renderer.FillRect(left, cardTop, right, cardTop + 140.0f, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, cardTop, right, cardTop + 140.0f, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"System Audio Loopback", left + 20.0f, cardTop + 16.0f, left + 250.0f, cardTop + 36.0f, Theme::TextPrimary, 13.0f, true);
        m_renderer.DrawVuMeter(left + 20.0f, cardTop + 40.0f, right - 20.0f, cardTop + 54.0f, sysVu);

        m_renderer.DrawText(L"Microphone Capture", left + 20.0f, cardTop + 72.0f, left + 250.0f, cardTop + 92.0f, Theme::TextPrimary, 13.0f, true);
        m_renderer.DrawVuMeter(left + 20.0f, cardTop + 96.0f, right - 20.0f, cardTop + 110.0f, micVu);
    }

    void MainWindow::RenderHotkeysPanel(float left, float top, float right, float /*bottom*/) {
        m_renderer.DrawText(L"Global Hotkeys Configuration", left, top, right, top + 30.0f, Theme::TextPrimary, 18.0f, true);

        const auto& settings = Config::SettingsManager::Instance().Get();

        float cardTop = top + 50.0f;
        m_renderer.FillRect(left, cardTop, right, cardTop + 200.0f, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, cardTop, right, cardTop + 200.0f, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Start / Stop Recording:  " + settings.hotkeys.startStop, left + 20.0f, cardTop + 20.0f, right - 20.0f, cardTop + 42.0f, Theme::TextPrimary, 13.0f);
        m_renderer.DrawText(L"Pause / Resume:          " + settings.hotkeys.pauseResume, left + 20.0f, cardTop + 50.0f, right - 20.0f, cardTop + 72.0f, Theme::TextPrimary, 13.0f);
        m_renderer.DrawText(L"Save Instant Replay:     " + settings.hotkeys.saveReplay, left + 20.0f, cardTop + 80.0f, right - 20.0f, cardTop + 102.0f, Theme::TextPrimary, 13.0f);
        m_renderer.DrawText(L"Mute / Unmute Mic:       " + settings.hotkeys.muteMic, left + 20.0f, cardTop + 110.0f, right - 20.0f, cardTop + 132.0f, Theme::TextPrimary, 13.0f);
        m_renderer.DrawText(L"Take Screenshot:         " + settings.hotkeys.screenshot, left + 20.0f, cardTop + 140.0f, right - 20.0f, cardTop + 162.0f, Theme::TextPrimary, 13.0f);
    }

    void MainWindow::RenderOutputPanel(float left, float top, float right, float /*bottom*/) {
        m_renderer.DrawText(L"Output Directory & Templating", left, top, right, top + 30.0f, Theme::TextPrimary, 18.0f, true);

        const auto& settings = Config::SettingsManager::Instance().Get();

        float cardTop = top + 50.0f;
        m_renderer.FillRect(left, cardTop, right, cardTop + 160.0f, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, cardTop, right, cardTop + 160.0f, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Destination Directory:", left + 20.0f, cardTop + 20.0f, right - 20.0f, cardTop + 40.0f, Theme::TextPrimary, 13.0f, true);
        m_renderer.DrawText(settings.output.destinationDirectory, left + 20.0f, cardTop + 44.0f, right - 20.0f, cardTop + 66.0f, Theme::AccentBlue, 13.0f);

        m_renderer.DrawText(L"Filename Template:", left + 20.0f, cardTop + 80.0f, right - 20.0f, cardTop + 100.0f, Theme::TextPrimary, 13.0f, true);
        m_renderer.DrawText(settings.output.filenameTemplate + L" (Auto-remux to MP4 enabled)", left + 20.0f, cardTop + 104.0f, right - 20.0f, cardTop + 126.0f, Theme::TextSecondary, 13.0f);
    }

    void MainWindow::RenderLibraryPanel(float left, float top, float right, float /*bottom*/) {
        m_renderer.DrawText(L"Recordings Library", left, top, right, top + 30.0f, Theme::TextPrimary, 18.0f, true);
        m_renderer.DrawText(L"Past recordings are listed here. Click to open destination folder or copy file to clipboard.",
                            left, top + 32.0f, right, top + 54.0f, Theme::TextSecondary, 13.0f);

        float cardTop = top + 60.0f;
        m_renderer.FillRect(left, cardTop, right, cardTop + 80.0f, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, cardTop, right, cardTop + 80.0f, Theme::BorderSubtle, 1.0f, 8.0f);

        m_renderer.DrawText(L"Recent Captures Folder is ready", left + 20.0f, cardTop + 20.0f, right - 20.0f, cardTop + 42.0f, Theme::TextPrimary, 14.0f, true);
        m_renderer.DrawText(L"Click [Output Paths] tab to view or change destination directory.", left + 20.0f, cardTop + 44.0f, right - 20.0f, cardTop + 66.0f, Theme::TextMuted, 12.0f);
    }

    void MainWindow::RenderDiagnosticsPanel(float left, float top, float right, float /*bottom*/) {
        m_renderer.DrawText(L"Diagnostics & Performance Telemetry", left, top, right, top + 30.0f, Theme::TextPrimary, 18.0f, true);

        auto telem = Core::Engine::Instance().GetTelemetry();
        auto logs = Core::Engine::Instance().GetDroppedFrameLogs();

        float cardTop = top + 50.0f;
        m_renderer.FillRect(left, cardTop, right, cardTop + 180.0f, Theme::SurfaceCard, 8.0f);
        m_renderer.DrawRect(left, cardTop, right, cardTop + 180.0f, Theme::BorderSubtle, 1.0f, 8.0f);

        wchar_t buf[256];
        swprintf_s(buf, L"• Recorder CPU Usage: %.2f%%\n• Total Frames Captured: %llu\n• Total Frames Encoded: %llu\n• Dropped Frames: %llu\n• Bytes Written: %llu MB\n• Free Disk Space: %llu GB",
                   telem.recorderCpuPercent, telem.totalFramesCaptured, telem.totalFramesEncoded,
                   telem.droppedFrames, telem.bytesWritten / (1024 * 1024), telem.freeDiskSpaceMb / 1024);
        m_renderer.DrawText(buf, left + 20.0f, cardTop + 20.0f, right - 20.0f, cardTop + 160.0f, Theme::TextSecondary, 13.0f);
    }

    void MainWindow::OnClick(int x, int y) {
        // Sidebar tab clicks
        if (x < 200) {
            float startY = 80.0f;
            float itemHeight = 42.0f;
            int clickedIdx = static_cast<int>((y - startY) / (itemHeight + 6.0f));
            if (clickedIdx >= 0 && clickedIdx <= 6) {
                m_activeTab = static_cast<NavigationTab>(clickedIdx);
            }
            return;
        }

        // Header Record button click
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        float right = static_cast<float>(rc.right - rc.left);
        float btnRight = right - 24.0f;
        float btnLeft = btnRight - 150.0f;
        float btnTop = 14.0f;
        float btnBottom = 56.0f;

        if (x >= btnLeft && x <= btnRight && y >= btnTop && y <= btnBottom) {
            auto& engine = Core::Engine::Instance();
            if (engine.GetState() == Core::EngineState::Idle) {
                Core::CaptureSourceDescriptor src;
                if (!m_cachedSources.empty() && m_selectedSourceIndex < static_cast<int>(m_cachedSources.size())) {
                    src = m_cachedSources[m_selectedSourceIndex];
                }
                engine.StartRecording(src);
            } else if (engine.GetState() == Core::EngineState::Recording || engine.GetState() == Core::EngineState::Paused) {
                engine.StopRecording();
            }
            return;
        }

        // Source card click selection
        if (m_activeTab == NavigationTab::Sources && y > 120) {
            float cardY = 120.0f;
            float cardWidth = 320.0f;
            float cardHeight = 70.0f;
            float left = 224.0f;

            for (size_t i = 0; i < m_cachedSources.size() && i < 6; ++i) {
                float cLeft = left + (i % 2) * (cardWidth + 16.0f);
                float cTop = cardY + (i / 2) * (cardHeight + 12.0f);
                float cRight = cLeft + cardWidth;
                float cBottom = cTop + cardHeight;

                if (x >= cLeft && x <= cRight && y >= cTop && y <= cBottom) {
                    m_selectedSourceIndex = static_cast<int>(i);
                    break;
                }
            }
        }
    }
#endif

} // namespace Recorder::Ui
