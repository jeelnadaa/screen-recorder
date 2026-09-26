#include "audio/WasapiAudioEngine.h"
#include <iostream>
#include <vector>

#if defined(_WIN32)
#include <avrt.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "avrt.lib")
#endif

namespace Recorder::Audio {

    WasapiAudioEngine::WasapiAudioEngine() = default;

    WasapiAudioEngine::~WasapiAudioEngine() {
        Stop();
    }

    bool WasapiAudioEngine::Initialize(const Core::AudioConfig& config) {
        m_config = config;
        m_systemVolume.store(config.systemAudioVolume);
        m_micVolume.store(config.micVolume);
        m_micMuted.store(false);

        return m_encoder.Initialize(config);
    }

    void WasapiAudioEngine::SetPacketCallback(AudioPacketCallback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callback = std::move(callback);
    }

    void WasapiAudioEngine::SetSystemVolume(float volume) {
        m_systemVolume.store(volume);
    }

    void WasapiAudioEngine::SetMicVolume(float volume) {
        m_micVolume.store(volume);
    }

    void WasapiAudioEngine::SetMicMuted(bool muted) {
        m_micMuted.store(muted);
    }

    bool WasapiAudioEngine::IsMicMuted() const {
        return m_micMuted.load();
    }

    float WasapiAudioEngine::GetSystemVuLevel() const {
        return m_mixer.GetSystemPeak();
    }

    float WasapiAudioEngine::GetMicVuLevel() const {
        return m_mixer.GetMicPeak();
    }

    bool WasapiAudioEngine::IsCapturing() const {
        return m_isCapturing.load();
    }

    bool WasapiAudioEngine::Start() {
        if (m_isCapturing.exchange(true)) return true;

#if defined(_WIN32)
        m_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        IMMDeviceEnumerator* pEnumerator = nullptr;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&pEnumerator));
        if (SUCCEEDED(hr)) {
            // 1. Initialize System Loopback
            if (m_config.systemAudioEnabled) {
                IMMDevice* pRenderDevice = nullptr;
                if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pRenderDevice))) {
                    if (SUCCEEDED(pRenderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&m_systemAudioClient)))) {
                        WAVEFORMATEX* pwfx = nullptr;
                        if (SUCCEEDED(m_systemAudioClient->GetMixFormat(&pwfx))) {
                            hr = m_systemAudioClient->Initialize(
                                AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_LOOPBACK,
                                10'000'000, // 1 second buffer
                                0,
                                pwfx,
                                nullptr
                            );
                            if (SUCCEEDED(hr)) {
                                m_systemAudioClient->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&m_systemCaptureClient));
                                m_systemAudioClient->Start();
                            }
                            CoTaskMemFree(pwfx);
                        }
                    }
                    pRenderDevice->Release();
                }
            }

            // 2. Initialize Microphone
            if (m_config.micEnabled) {
                IMMDevice* pCaptureDevice = nullptr;
                if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pCaptureDevice))) {
                    if (SUCCEEDED(pCaptureDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&m_micAudioClient)))) {
                        WAVEFORMATEX* pwfx = nullptr;
                        if (SUCCEEDED(m_micAudioClient->GetMixFormat(&pwfx))) {
                            hr = m_micAudioClient->Initialize(
                                AUDCLNT_SHAREMODE_SHARED,
                                0,
                                10'000'000,
                                0,
                                pwfx,
                                nullptr
                            );
                            if (SUCCEEDED(hr)) {
                                m_micAudioClient->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&m_micCaptureClient));
                                m_micAudioClient->Start();
                            }
                            CoTaskMemFree(pwfx);
                        }
                    }
                    pCaptureDevice->Release();
                }
            }

            pEnumerator->Release();
        }

        m_pumpThread = std::thread(&WasapiAudioEngine::AudioPumpThread, this);
        return true;
#else
        return true;
#endif
    }

    void WasapiAudioEngine::Stop() {
        if (!m_isCapturing.exchange(false)) return;

#if defined(_WIN32)
        if (m_stopEvent) {
            SetEvent(m_stopEvent);
        }

        if (m_pumpThread.joinable()) {
            m_pumpThread.join();
        }

        if (m_systemAudioClient) {
            m_systemAudioClient->Stop();
            m_systemAudioClient->Release();
            m_systemAudioClient = nullptr;
        }
        if (m_systemCaptureClient) {
            m_systemCaptureClient->Release();
            m_systemCaptureClient = nullptr;
        }

        if (m_micAudioClient) {
            m_micAudioClient->Stop();
            m_micAudioClient->Release();
            m_micAudioClient = nullptr;
        }
        if (m_micCaptureClient) {
            m_micCaptureClient->Release();
            m_micCaptureClient = nullptr;
        }

        if (m_stopEvent) {
            CloseHandle(m_stopEvent);
            m_stopEvent = nullptr;
        }
#endif
    }

    void WasapiAudioEngine::AudioPumpThread() {
#if defined(_WIN32)
        DWORD taskIndex = 0;
        HANDLE hMmcss = AvSetMmThreadCharacteristicsW(L"Audio", &taskIndex);

        std::vector<float> mixedOutput;

        while (m_isCapturing) {
            BYTE* pSysData = nullptr;
            UINT32 sysFrames = 0;
            DWORD sysFlags = 0;

            BYTE* pMicData = nullptr;
            UINT32 micFrames = 0;
            DWORD micFlags = 0;

            if (m_systemCaptureClient) {
                m_systemCaptureClient->GetBuffer(&pSysData, &sysFrames, &sysFlags, nullptr, nullptr);
            }
            if (m_micCaptureClient) {
                m_micCaptureClient->GetBuffer(&pMicData, &micFrames, &micFlags, nullptr, nullptr);
            }

            const float* pSysFloat = (sysFlags & AUDCLNT_BUFFERFLAGS_SILENT) ? nullptr : reinterpret_cast<const float*>(pSysData);
            const float* pMicFloat = (micFlags & AUDCLNT_BUFFERFLAGS_SILENT) ? nullptr : reinterpret_cast<const float*>(pMicData);

            if (sysFrames > 0 || micFrames > 0) {
                m_mixer.Mix(
                    pSysFloat, sysFrames, m_systemVolume.load(),
                    pMicFloat, micFrames, m_micVolume.load(), m_micMuted.load(),
                    mixedOutput, m_config.channels
                );

                LARGE_INTEGER qpc;
                QueryPerformanceCounter(&qpc);
                int64_t timestampHns = qpc.QuadPart;

                std::vector<Core::MediaPacket> packets;
                if (m_encoder.Encode(mixedOutput.data(), mixedOutput.size(), timestampHns, packets)) {
                    AudioPacketCallback cb;
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        cb = m_callback;
                    }

                    if (cb) {
                        for (auto& pkt : packets) {
                            cb(std::move(pkt));
                        }
                    }
                }
            }

            if (m_systemCaptureClient && sysFrames > 0) {
                m_systemCaptureClient->ReleaseBuffer(sysFrames);
            }
            if (m_micCaptureClient && micFrames > 0) {
                m_micCaptureClient->ReleaseBuffer(micFrames);
            }

            // Sleep 10ms between audio reads
            WaitForSingleObject(m_stopEvent, 10);
        }

        if (hMmcss) {
            AvRevertMmThreadCharacteristics(hMmcss);
        }
#endif
    }

} // namespace Recorder::Audio
