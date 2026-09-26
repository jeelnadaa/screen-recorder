#pragma once

#include "UITheme.h"
#include "Direct2DRenderer.h"
#include "TrayIcon.h"
#include "../core/Types.h"
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Recorder {
namespace Ui {

    struct LibraryItem {
        std::wstring filename;
        std::wstring filepath;
        uint64_t fileSize = 0;
        std::wstring resFps;
        std::wstring timeAgo;
    };

    class MainWindow {
    public:
        MainWindow();
        ~MainWindow();

        bool Create(int width = 940, int height = 620);
        void Show();
        void RunMessageLoop();

        void RefreshLibrary();
        void TakeScreenshot();
        void BrowseOutputDirectory();
        void OpenOutputDirectory();

    private:
#if defined(_WIN32)
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        void RegisterGlobalHotkeys();
        void UnregisterGlobalHotkeys();

        void Render();
        void RenderSidebar(float width, float height);
        void RenderTopBar(float left, float top, float right, float height);

        void RenderSourcePanel(float left, float top, float right, float bottom);
        void RenderVideoPanel(float left, float top, float right, float bottom);
        void RenderAudioPanel(float left, float top, float right, float bottom);
        void RenderHotkeysPanel(float left, float top, float right, float bottom);
        void RenderOutputPanel(float left, float top, float right, float bottom);
        void RenderLibraryPanel(float left, float top, float right, float bottom);
        void RenderAdvancedPanel(float left, float top, float right, float bottom);
        void RenderDropdowns(float left, float top, float right, float bottom);

        void OnClick(float x, float y);
        void OnMouseMove(float x, float y);

        HWND m_hwnd = nullptr;
        Direct2DRenderer m_renderer;
        TrayIcon m_trayIcon;
        float m_dpi = 96.0f;
#endif

        NavigationTab m_activeTab = NavigationTab::Source;
        SourceMode m_sourceMode = SourceMode::Monitor;

        int m_selectedSourceIndex = 0;
        int m_selectedResIndex = 0;
        int m_selectedFpsIndex = 2; // 60 fps

        // Video Tab State
        int m_selectedCodecIndex = 0; // 0=HEVC, 1=AV1, 2=H264
        int m_selectedBitrateIndex = 1; // 0=15M, 1=25M, 2=40M, 3=60M
        int m_selectedRateControlIndex = 0; // 0=CFR, 1=VFR
        int m_selectedBitrateModeIndex = 0; // 0=CBR, 1=VBR, 2=CQP
        bool m_strictHardwarePolicy = true;

        // Audio Tab State
        bool m_systemAudioEnabled = true;
        float m_systemVolume = 0.85f;
        bool m_micEnabled = true;
        float m_micVolume = 0.75f;
        bool m_perProcessAudio = false;
        int m_audioBitrateIndex = 1; // 0=128k, 1=192k, 2=320k

        // Hotkeys Tab State
        int m_hotkeyProfileIndex = 0; // 0=Standard, 1=F-Keys, 2=Alt-Combos

        // Output Tab State
        int m_selectedTemplateIndex = 0;
        bool m_autoRemuxMp4 = true;
        bool m_deleteMkvAfterRemux = true;

        // Advanced Tab State
        int m_replayDurationIndex = 1; // 0=30s, 1=60s, 2=120s, 3=300s
        bool m_d3d11Multithreading = true;

        // Dropdown popups
        bool m_sourceDropdownOpen = false;
        bool m_resDropdownOpen = false;
        bool m_fpsDropdownOpen = false;

        // Hover & Animation tracking
        int m_hoveredTab = -1;
        int m_hoveredSourceMode = -1;
        bool m_hoveredRecordBtn = false;
        bool m_hoveredScreenshotBtn = false;
        std::string m_hoveredControl = "";
        uint32_t m_animTick = 0;

        std::vector<Core::CaptureSourceDescriptor> m_cachedSources;
        std::vector<LibraryItem> m_libraryItems;
        std::wstring m_searchQuery;
    };

} // namespace Ui
} // namespace Recorder
