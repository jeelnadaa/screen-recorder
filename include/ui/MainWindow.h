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

        bool Create(int width = 860, int height = 580);
        void Show();
        void RunMessageLoop();

        void RefreshLibrary();
        void TakeScreenshot();

    private:
#if defined(_WIN32)
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        void RegisterGlobalHotkeys();
        void UnregisterGlobalHotkeys();

        void Render();
        void RenderSidebar(float width, float height);
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
        int m_selectedFpsIndex = 2; // 60 fps default

        bool m_sourceDropdownOpen = false;
        bool m_resDropdownOpen = false;
        bool m_fpsDropdownOpen = false;

        int m_hoveredTab = -1;
        int m_hoveredSourceMode = -1;
        bool m_hoveredRecordBtn = false;
        bool m_hoveredScreenshotBtn = false;

        std::vector<Core::CaptureSourceDescriptor> m_cachedSources;
        std::vector<LibraryItem> m_libraryItems;
        std::wstring m_searchQuery;
    };

} // namespace Ui
} // namespace Recorder
