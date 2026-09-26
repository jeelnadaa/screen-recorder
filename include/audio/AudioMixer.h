#pragma once

#include <vector>
#include <cstdint>
#include <atomic>

namespace Recorder {
namespace Audio {

    class AudioMixer {
    public:
        AudioMixer();

        void Mix(
            const float* systemBuffer, size_t systemFrames, float systemVolume,
            const float* micBuffer, size_t micFrames, float micVolume, bool micMuted,
            std::vector<float>& outputBuffer, size_t channels = 2
        );

        float GetSystemPeak() const { return m_systemPeak.load(); }
        float GetMicPeak() const { return m_micPeak.load(); }

    private:
        std::atomic<float> m_systemPeak{ 0.0f };
        std::atomic<float> m_micPeak{ 0.0f };
    };

} // namespace Audio
} // namespace Recorder
