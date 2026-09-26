#include "core/Engine.h"
#include "ui/MainWindow.h"
#include "ipc/NamedPipeServer.h"
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

int WINAPI wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
#if defined(_WIN32)
    // 1. Handle command-line arguments and CLI automation
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    bool startMinimized = false;

    if (argc > 1) {
        std::wstring arg1 = argv[1];
        std::string response;

        if (arg1 == L"--start-record" || arg1 == L"-r") {
            if (Recorder::Ipc::NamedPipeServer::SendCommand("START", response)) {
                LocalFree(argv);
                return 0;
            }
        } else if (arg1 == L"--stop-record" || arg1 == L"-s") {
            if (Recorder::Ipc::NamedPipeServer::SendCommand("STOP", response)) {
                LocalFree(argv);
                return 0;
            }
        } else if (arg1 == L"--save-replay") {
            if (Recorder::Ipc::NamedPipeServer::SendCommand("REPLAY", response)) {
                LocalFree(argv);
                return 0;
            }
        } else if (arg1 == L"--status") {
            if (Recorder::Ipc::NamedPipeServer::SendCommand("STATUS", response)) {
                LocalFree(argv);
                return 0;
            }
        } else if (arg1 == L"--minimized" || arg1 == L"-m") {
            startMinimized = true;
        }
    }
    LocalFree(argv);

    // 2. Single-Instance Check
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\ScreenRecorderAppSingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // App is already running; foreground the existing window
        HWND existingWnd = FindWindowW(L"ScreenRecorderMainWindowClass", nullptr);
        if (existingWnd) {
            ShowWindow(existingWnd, SW_RESTORE);
            SetForegroundWindow(existingWnd);
        }
        CloseHandle(hMutex);
        return 0;
    }

    // 3. Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        CloseHandle(hMutex);
        return 1;
    }

    // 4. Initialize Core Engine
    if (!Recorder::Core::Engine::Instance().Initialize()) {
        MessageBoxW(nullptr, L"Failed to initialize Screen Recording Engine.", L"Initialization Error", MB_ICONERROR | MB_OK);
        CoUninitialize();
        CloseHandle(hMutex);
        return 1;
    }

    // 5. Initialize UI
    Recorder::Ui::MainWindow mainWindow;
    if (mainWindow.Create(960, 640)) {
        if (!startMinimized) {
            mainWindow.Show();
        }
        mainWindow.RunMessageLoop();
    }

    // 6. Clean Shutdown
    Recorder::Core::Engine::Instance().Shutdown();
    CoUninitialize();
    CloseHandle(hMutex);
#endif

    return 0;
}
