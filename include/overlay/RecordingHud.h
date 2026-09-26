#pragma once

#include <string>
#include <cstdint>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Recorder::Overlay {

    class RecordingHud {
    public:
        RecordingHud();
        ~RecordingHud();

        bool Create();
        void Destroy();

        void Show();
        void Hide();
        void Update(uint64_t elapsedMs, bool isPaused);

    private:
#if defined(_WIN32)
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        HWND m_hwnd = nullptr;
#endif
        bool m_isPaused = false;
        uint64_t m_elapsedMs = 0;
    };

} // namespace Recorder::Overlay
