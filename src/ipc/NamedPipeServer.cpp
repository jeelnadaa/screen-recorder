#include "ipc/NamedPipeServer.h"
#include <iostream>

namespace Recorder::Ipc {

    NamedPipeServer::NamedPipeServer() = default;

    NamedPipeServer::~NamedPipeServer() {
        Stop();
    }

    bool NamedPipeServer::Start(CommandHandler handler, const std::wstring& pipeName) {
        if (m_running.exchange(true)) return true;

        m_pipeName = pipeName;
        m_handler = std::move(handler);

#if defined(_WIN32)
        m_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        m_thread = std::thread(&NamedPipeServer::ServerLoop, this);
        return true;
#else
        return true;
#endif
    }

    void NamedPipeServer::Stop() {
        if (!m_running.exchange(false)) return;

#if defined(_WIN32)
        if (m_stopEvent) {
            SetEvent(m_stopEvent);
        }

        // Connect once to unblock ConnectNamedPipe
        HANDLE hDummy = CreateFileW(
            m_pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_EXISTING, 0, nullptr
        );
        if (hDummy != INVALID_HANDLE_VALUE) {
            CloseHandle(hDummy);
        }

        if (m_thread.joinable()) {
            m_thread.join();
        }

        if (m_stopEvent) {
            CloseHandle(m_stopEvent);
            m_stopEvent = nullptr;
        }
#endif
    }

    void NamedPipeServer::ServerLoop() {
#if defined(_WIN32)
        while (m_running) {
            m_pipe = CreateNamedPipeW(
                m_pipeName.c_str(),
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                4096, 4096, 0, nullptr
            );

            if (m_pipe == INVALID_HANDLE_VALUE) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            BOOL connected = ConnectNamedPipe(m_pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

            if (connected && m_running) {
                char buffer[1024] = { 0 };
                DWORD bytesRead = 0;
                if (ReadFile(m_pipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                    std::string cmd(buffer, bytesRead);
                    std::string response = m_handler ? m_handler(cmd) : "OK\n";

                    DWORD bytesWritten = 0;
                    WriteFile(m_pipe, response.data(), static_cast<DWORD>(response.size()), &bytesWritten, nullptr);
                }

                FlushFileBuffers(m_pipe);
                DisconnectNamedPipe(m_pipe);
            }

            CloseHandle(m_pipe);
            m_pipe = INVALID_HANDLE_VALUE;
        }
#endif
    }

    bool NamedPipeServer::SendCommand(const std::string& cmd, std::string& outResponse, const std::wstring& pipeName) {
#if defined(_WIN32)
        HANDLE hPipe = INVALID_HANDLE_VALUE;
        for (int attempts = 0; attempts < 30; ++attempts) {
            hPipe = CreateFileW(
                pipeName.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0, nullptr, OPEN_EXISTING, 0, nullptr
            );
            if (hPipe != INVALID_HANDLE_VALUE) {
                break;
            }
            if (GetLastError() == ERROR_PIPE_BUSY) {
                WaitNamedPipeW(pipeName.c_str(), 100);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }

        if (hPipe == INVALID_HANDLE_VALUE) {
            return false;
        }

        DWORD bytesWritten = 0;
        if (!WriteFile(hPipe, cmd.data(), static_cast<DWORD>(cmd.size()), &bytesWritten, nullptr)) {
            CloseHandle(hPipe);
            return false;
        }

        char buffer[1024] = { 0 };
        DWORD bytesRead = 0;
        if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
            outResponse.assign(buffer, bytesRead);
        }

        CloseHandle(hPipe);
        return true;
#else
        outResponse = "OK";
        return true;
#endif
    }

} // namespace Recorder::Ipc
