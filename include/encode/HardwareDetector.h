#pragma once

#include "EncoderTypes.h"

namespace Recorder::Encode {

    class HardwareDetector {
    public:
        static EncoderCapabilities DetectCapabilities();
        static bool HasHardwareEncoder(Core::VideoCodec codec);
    };

} // namespace Recorder::Encode
