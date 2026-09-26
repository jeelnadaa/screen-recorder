#pragma once

#include "../core/Types.h"
#include <string>

namespace Recorder::Encode {

    struct EncoderCapabilities {
        bool hasHardwareH264 = false;
        bool hasHardwareHEVC = false;
        bool hasHardwareAV1 = false;
        std::wstring hardwareVendorName = L"None";
        std::wstring primaryEncoderDescription;
    };

    enum class EncoderStatus {
        Ok,
        HardwareUnavailable,
        InitializationFailed,
        CodecNotSupported,
        OutOfMemory
    };

} // namespace Recorder::Encode
