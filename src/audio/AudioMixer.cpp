#include "audio/AudioMixer.h"
#include <algorithm>
#include <cmath>

namespace Recorder::Audio {

    AudioMixer::AudioMixer() = default;

    void AudioMixer::Mix(
        const float* systemBuffer, size_t systemFrames, float systemVolume,
        const float* micBuffer, size_t micFrames, float micVolume, bool micMuted,
        std::vector<float>& outputBuffer, size_t channels
    ) {
        size_t maxFrames = std::max(systemFrames, micFrames);
        size_t totalSamples = maxFrames * channels;
        outputBuffer.resize(totalSamples, 0.0f);

        float sysPeak = 0.0f;
        float micPeak = 0.0f;

        for (size_t i = 0; i < maxFrames; ++i) {
            for (size_t c = 0; c < channels; ++c) {
                float sample = 0.0f;

                // Add System Audio
                if (systemBuffer && i < systemFrames) {
                    float sysVal = systemBuffer[i * channels + c] * systemVolume;
                    sample += sysVal;
                    sysPeak = std::max(sysPeak, std::abs(sysVal));
                }

                // Add Microphone Audio
                if (micBuffer && !micMuted && i < micFrames) {
                    float micVal = micBuffer[i * channels + c] * micVolume;
                    sample += micVal;
                    micPeak = std::max(micPeak, std::abs(micVal));
                }

                // Hard-clipping protection
                if (sample > 1.0f) sample = 1.0f;
                else if (sample < -1.0f) sample = -1.0f;

                outputBuffer[i * channels + c] = sample;
            }
        }

        m_systemPeak.store(sysPeak);
        m_micPeak.store(micPeak);
    }

} // namespace Recorder::Audio
