#pragma once

#include "core/Types.h"
#include <functional>
#include <string>

namespace Recorder::Audio {

    using AudioPacketCallback = std::function<void(Core::MediaPacket&&)>;

    class IAudioEngine {
    public:
        virtual ~IAudioEngine() = default;

        virtual bool Initialize(const Core::AudioConfig& config) = 0;
        virtual bool Start() = 0;
        virtual void Stop() = 0;
        virtual bool IsCapturing() const = 0;

        virtual void SetPacketCallback(AudioPacketCallback callback) = 0;
        virtual void SetSystemVolume(float volume) = 0;
        virtual void SetMicVolume(float volume) = 0;
        virtual void SetMicMuted(bool muted) = 0;
        virtual bool IsMicMuted() const = 0;

        virtual float GetSystemVuLevel() const = 0;
        virtual float GetMicVuLevel() const = 0;
    };

} // namespace Recorder::Audio
