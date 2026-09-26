#pragma once

#include "core/Types.h"
#include "EncoderTypes.h"
#include <functional>

#if defined(_WIN32)
#include <d3d11.h>
#include <mfidl.h>
#endif

namespace Recorder::Encode {

    using EncodedPacketCallback = std::function<void(Core::MediaPacket&&)>;

    class IVideoEncoder {
    public:
        virtual ~IVideoEncoder() = default;

#if defined(_WIN32)
        virtual EncoderStatus Initialize(const Core::VideoConfig& config, ID3D11Device* device, IMFDXGIDeviceManager* dxgiManager) = 0;
        virtual bool SubmitFrame(ID3D11Texture2D* texture, int64_t timestampHns) = 0;
#endif
        virtual void SetPacketCallback(EncodedPacketCallback callback) = 0;
        virtual void Drain() = 0;
        virtual void Shutdown() = 0;

        virtual bool IsHardwareAccelerated() const = 0;
        virtual const char* GetEncoderName() const = 0;
    };

} // namespace Recorder::Encode
