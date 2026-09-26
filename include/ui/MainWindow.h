#pragma once

#include "UITheme.h"
#include "Direct2DRenderer.h"
#include "TrayIcon.h"
#include "core/Types.h"
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Recorder::Ui {

    class MainWindow {
    public:
        MainWindow();
        ~MainWindow();

        bool Create(int width = 940, int height = 620);
        void Show();
        void RunMessageLoop();

    private:
#if defined(_WIN32)
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        void RegisterGlobalHotkeys();
        void UnregisterGlobalHotkeys();

        void Render();
        void RenderSidebar(float width, float height);
        void RenderHeader(float left, float top, float right, float height);
        void RenderTabContent(float left, float top, float right, float bottom);
        
        void RenderSourcesPanel(float left, float top, float right, float bottom);
        void RenderVideoPanel(float left, float top, float right, float bottom);
        void RenderAudioPanel(float left, float top, float right, float bottom);
        void RenderHotkeysPanel(float left, float top, float right, float bottom);
        void RenderOutputPanel(float left, float top, float right, float bottom);
        void RenderLibraryPanel(float left, float top, float right, float bottom);
        void RenderDiagnosticsPanel(float left, float top, float right, float bottom);

        void OnClick(int x, int y);

        HWND m_hwnd = nullptr;
        Direct2DRenderer m_renderer;
        TrayIcon m_trayIcon;
#endif

        NavigationTab m_activeTab = NavigationTab::Sources;
        bool m_isHoveredRecordBtn = false;
        std::vector<Core::CaptureSourceDescriptor> m_cachedSources;
        int m_selectedSourceIndex = 0;
    };

} // namespace Recorder::Ui
