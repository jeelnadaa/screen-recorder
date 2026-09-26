#pragma once

#include "IVideoEncoder.h"
#include <mutex>
#include <atomic>

#if defined(_WIN32)
#include <mfapi.h>
#include <mftransform.h>
#include <codecapi.h>
#endif

namespace Recorder::Encode {

    class MftVideoEncoder : public IVideoEncoder {
    public:
        MftVideoEncoder();
        ~MftVideoEncoder() override;

#if defined(_WIN32)
        EncoderStatus Initialize(const Core::VideoConfig& config, ID3D11Device* device, IMFDXGIDeviceManager* dxgiManager) override;
        bool SubmitFrame(ID3D11Texture2D* texture, int64_t timestampHns) override;
#endif
        void SetPacketCallback(EncodedPacketCallback callback) override;
        void Drain() override;
        void Shutdown() override;

        bool IsHardwareAccelerated() const override { return m_isHardware; }
        const char* GetEncoderName() const override { return m_encoderName.c_str(); }

    private:
        void PullOutputSamples();

#if defined(_WIN32)
        IMFTransform* m_transform = nullptr;
        ID3D11Device* m_d3dDevice = nullptr;
        IMFDXGIDeviceManager* m_dxgiManager = nullptr;
        DWORD m_inputStreamId = 0;
        DWORD m_outputStreamId = 0;
#endif

        mutable std::mutex m_mutex;
        EncodedPacketCallback m_callback;
        Core::VideoConfig m_config;
        bool m_isHardware = true;
        std::string m_encoderName = "MFT Hardware Encoder";
        bool m_initialized = false;
        int64_t m_lastFrameTimeHns = 0;
        int64_t m_frameDurationHns = 166666; // 60fps default
    };

} // namespace Recorder::Encode
