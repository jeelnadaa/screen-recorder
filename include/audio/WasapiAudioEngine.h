#pragma once

#include "IAudioEngine.h"
#include "AudioMixer.h"
#include "MftAudioEncoder.h"
#include <thread>
#include <atomic>
#include <mutex>

#if defined(_WIN32)
#include <audioclient.h>
#include <mmdeviceapi.h>
#endif

namespace Recorder::Audio {

    class WasapiAudioEngine : public IAudioEngine {
    public:
        WasapiAudioEngine();
        ~WasapiAudioEngine() override;

        bool Initialize(const Core::AudioConfig& config) override;
        bool Start() override;
        void Stop() override;
        bool IsCapturing() const override;

        void SetPacketCallback(AudioPacketCallback callback) override;
        void SetSystemVolume(float volume) override;
        void SetMicVolume(float volume) override;
        void SetMicMuted(bool muted) override;
        bool IsMicMuted() const override;

        float GetSystemVuLevel() const override;
        float GetMicVuLevel() const override;

    private:
        void AudioPumpThread();

#if defined(_WIN32)
        IAudioClient* m_systemAudioClient = nullptr;
        IAudioCaptureClient* m_systemCaptureClient = nullptr;
        IAudioClient* m_micAudioClient = nullptr;
        IAudioCaptureClient* m_micCaptureClient = nullptr;
        HANDLE m_stopEvent = nullptr;
#endif

        Core::AudioConfig m_config;
        AudioMixer m_mixer;
        MftAudioEncoder m_encoder;

        mutable std::mutex m_mutex;
        AudioPacketCallback m_callback;
        std::atomic<bool> m_isCapturing{ false };
        std::atomic<bool> m_micMuted{ false };
        std::atomic<float> m_systemVolume{ 1.0f };
        std::atomic<float> m_micVolume{ 1.0f };
        std::thread m_pumpThread;
    };

} // namespace Recorder::Audio
