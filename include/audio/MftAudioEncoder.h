#pragma once

#include "core/Types.h"
#include <vector>
#include <mutex>

#if defined(_WIN32)
#include <mfapi.h>
#include <mftransform.h>
#endif

namespace Recorder::Audio {

    class MftAudioEncoder {
    public:
        MftAudioEncoder();
        ~MftAudioEncoder();

        bool Initialize(const Core::AudioConfig& config);
        void Shutdown();

        bool Encode(const float* pcmData, size_t sampleCount, int64_t timestampHns, std::vector<Core::MediaPacket>& outPackets);

    private:
#if defined(_WIN32)
        IMFTransform* m_transform = nullptr;
#endif
        Core::AudioConfig m_config;
        bool m_initialized = false;
        std::mutex m_mutex;
    };

} // namespace Recorder::Audio
