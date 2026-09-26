#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Recorder::Ipc {

    using CommandHandler = std::function<std::string(const std::string& cmd)>;

    class NamedPipeServer {
    public:
        NamedPipeServer();
        ~NamedPipeServer();

        bool Start(CommandHandler handler, const std::wstring& pipeName = L"\\\\.\\pipe\\ScreenRecorderCmd");
        void Stop();

        static bool SendCommand(const std::string& cmd, std::string& outResponse, const std::wstring& pipeName = L"\\\\.\\pipe\\ScreenRecorderCmd");

    private:
        void ServerLoop();

#if defined(_WIN32)
        HANDLE m_pipe = INVALID_HANDLE_VALUE;
        HANDLE m_stopEvent = nullptr;
#endif
        std::wstring m_pipeName;
        CommandHandler m_handler;
        std::atomic<bool> m_running{ false };
        std::thread m_thread;
    };

} // namespace Recorder::Ipc
