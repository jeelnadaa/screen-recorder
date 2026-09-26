#pragma once

#include "../core/Types.h"
#include <string>
#include <cstdint>

namespace Recorder::Mux {

    struct MuxerConfig {
        std::wstring outputPath;
        Core::VideoConfig video;
        Core::AudioConfig audio;
        uint32_t autoSplitSizeMb = 0;
        uint32_t autoSplitDurationMin = 0;
    };

    struct RemuxResult {
        bool success = false;
        std::wstring mp4Path;
        uint64_t fileSizeBytes = 0;
        uint64_t remuxDurationMs = 0;
        std::wstring errorMessage;
    };

} // namespace Recorder::Mux
