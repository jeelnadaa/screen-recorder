#include "encode/MftVideoEncoder.h"
#include <iostream>

#if defined(_WIN32)
#include <wmcodecdsp.h>
#pragma comment(lib, "wmcodecdspuuid.lib")
#endif

namespace Recorder::Encode {

    MftVideoEncoder::MftVideoEncoder() = default;

    MftVideoEncoder::~MftVideoEncoder() {
        Shutdown();
    }

    void MftVideoEncoder::SetPacketCallback(EncodedPacketCallback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callback = std::move(callback);
    }

    void MftVideoEncoder::Shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
#if defined(_WIN32)
        if (m_transform) {
            m_transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
            m_transform->Release();
            m_transform = nullptr;
        }
#endif
        m_initialized = false;
    }

    void MftVideoEncoder::Drain() {
        std::lock_guard<std::mutex> lock(m_mutex);
#if defined(_WIN32)
        if (m_transform) {
            m_transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
            PullOutputSamples();
        }
#endif
    }

#if defined(_WIN32)
    EncoderStatus MftVideoEncoder::Initialize(const Core::VideoConfig& config, ID3D11Device* device, IMFDXGIDeviceManager* dxgiManager) {
        Shutdown();
        std::lock_guard<std::mutex> lock(m_mutex);

        m_config = config;
        m_d3dDevice = device;
        m_dxgiManager = dxgiManager;

        if (m_config.targetFps > 0) {
            m_frameDurationHns = 10'000'000LL / m_config.targetFps;
        }

        MFStartup(MF_VERSION);

        GUID targetSubtype = MFVideoFormat_H264;
        if (m_config.codec == Core::VideoCodec::HEVC) {
            targetSubtype = MFVideoFormat_HEVC;
        } else if (m_config.codec == Core::VideoCodec::AV1) {
            targetSubtype = MFVideoFormat_AV1;
        }

        MFT_REGISTER_TYPE_INFO outputTypeInfo = { MFMediaType_Video, targetSubtype };
        IMFActivate** ppActivate = nullptr;
        UINT32 count = 0;

        UINT32 flags = MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER;
        HRESULT hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, flags, nullptr, &outputTypeInfo, &ppActivate, &count);

        if (FAILED(hr) || count == 0 || !ppActivate) {
            if (!m_config.allowSoftwareFallback) {
                return EncoderStatus::HardwareUnavailable;
            }

            // Software compatibility mode fallback
            flags = MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER;
            hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, flags, nullptr, &outputTypeInfo, &ppActivate, &count);
            if (FAILED(hr) || count == 0 || !ppActivate) {
                return EncoderStatus::InitializationFailed;
            }
            m_isHardware = false;
            m_encoderName = "Software MFT (Compatibility Mode)";
        } else {
            m_isHardware = true;
            m_encoderName = "Hardware MFT (GPU Accelerated)";
        }

        hr = ppActivate[0]->ActivateObject(__uuidof(IMFTransform), reinterpret_cast<void**>(&m_transform));
        for (UINT32 i = 0; i < count; ++i) {
            ppActivate[i]->Release();
        }
        CoTaskMemFree(ppActivate);

        if (FAILED(hr) || !m_transform) {
            return EncoderStatus::InitializationFailed;
        }

        // Enable D3D11 hardware acceleration in the MFT encoder
        if (m_dxgiManager && m_isHardware) {
            hr = m_transform->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(m_dxgiManager));
            if (FAILED(hr)) {
                // Some hardware encoders accept it during stream configuration
            }
        }

        // Set Output Media Type
        IMFMediaType* pOutputType = nullptr;
        hr = MFCreateMediaType(&pOutputType);
        if (SUCCEEDED(hr)) {
            pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            pOutputType->SetGUID(MF_MT_SUBTYPE, targetSubtype);
            pOutputType->SetUINT32(MF_MT_AVG_BITRATE, m_config.targetBitrateKbps * 1000);
            MFSetAttributeSize(pOutputType, MF_MT_FRAME_SIZE, m_config.width, m_config.height);
            MFSetAttributeRatio(pOutputType, MF_MT_FRAME_RATE, m_config.targetFps, 1);
            MFSetAttributeRatio(pOutputType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
            pOutputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

            hr = m_transform->SetOutputType(0, pOutputType, 0);
            pOutputType->Release();
        }

        // Set Input Media Type (NV12 or ARGB)
        IMFMediaType* pInputType = nullptr;
        hr = MFCreateMediaType(&pInputType);
        if (SUCCEEDED(hr)) {
            pInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            pInputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
            MFSetAttributeSize(pInputType, MF_MT_FRAME_SIZE, m_config.width, m_config.height);
            MFSetAttributeRatio(pInputType, MF_MT_FRAME_RATE, m_config.targetFps, 1);
            MFSetAttributeRatio(pInputType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
            pInputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

            hr = m_transform->SetInputType(0, pInputType, 0);
            if (FAILED(hr)) {
                // Fallback to ARGB32
                pInputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);
                m_transform->SetInputType(0, pInputType, 0);
            }
            pInputType->Release();
        }

        // Send streaming start message
        m_transform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        m_transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        m_transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

        m_initialized = true;
        return EncoderStatus::Ok;
    }

    bool MftVideoEncoder::SubmitFrame(ID3D11Texture2D* texture, int64_t timestampHns) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized || !m_transform || !texture) return false;

        // CFR Frame Regulation: Ensure constant frame intervals if CFR is selected
        int64_t sampleTime = timestampHns;
        if (m_config.rateControlMode == Core::RateControlMode::CFR) {
            if (m_lastFrameTimeHns > 0) {
                sampleTime = m_lastFrameTimeHns + m_frameDurationHns;
            }
            m_lastFrameTimeHns = sampleTime;
        }

        IMFMediaBuffer* pBuffer = nullptr;
        HRESULT hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), texture, 0, FALSE, &pBuffer);
        if (FAILED(hr)) return false;

        IMFSample* pSample = nullptr;
        hr = MFCreateSample(&pSample);
        if (SUCCEEDED(hr)) {
            pSample->AddBuffer(pBuffer);
            pSample->SetSampleTime(sampleTime);
            pSample->SetSampleDuration(m_frameDurationHns);

            hr = m_transform->ProcessInput(0, pSample, 0);
            pSample->Release();
        }
        pBuffer->Release();

        if (SUCCEEDED(hr)) {
            PullOutputSamples();
            return true;
        }

        return false;
    }

    void MftVideoEncoder::PullOutputSamples() {
        if (!m_transform) return;

        MFT_OUTPUT_DATA_BUFFER outputDataBuffer = {};
        DWORD processOutputStatus = 0;

        MFT_OUTPUT_STREAM_INFO streamInfo = {};
        m_transform->GetOutputStreamInfo(0, &streamInfo);

        IMFSample* pSample = nullptr;
        MFCreateSample(&pSample);

        IMFMediaBuffer* pBuffer = nullptr;
        MFCreateMemoryBuffer(streamInfo.cbSize > 0 ? streamInfo.cbSize : 1024 * 1024, &pBuffer);
        pSample->AddBuffer(pBuffer);
        pBuffer->Release();

        outputDataBuffer.pSample = pSample;

        while (true) {
            HRESULT hr = m_transform->ProcessOutput(0, 1, &outputDataBuffer, &processOutputStatus);
            if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
                break;
            }
            if (FAILED(hr)) {
                break;
            }

            if (outputDataBuffer.pSample) {
                LONGLONG sampleTime = 0;
                LONGLONG sampleDuration = 0;
                outputDataBuffer.pSample->GetSampleTime(&sampleTime);
                outputDataBuffer.pSample->GetSampleDuration(&sampleDuration);

                UINT32 isKey = 0;
                outputDataBuffer.pSample->GetUINT32(MFSampleExtension_CleanPoint, &isKey);

                IMFMediaBuffer* pMediaBuffer = nullptr;
                if (SUCCEEDED(outputDataBuffer.pSample->ConvertToContiguousBuffer(&pMediaBuffer))) {
                    BYTE* pData = nullptr;
                    DWORD currentLength = 0;
                    if (SUCCEEDED(pMediaBuffer->Lock(&pData, nullptr, &currentLength))) {
                        Core::MediaPacket pkt;
                        pkt.type = isKey ? Core::PacketType::VideoKeyframe : Core::PacketType::VideoDeltaFrame;
                        pkt.isKeyframe = (isKey != 0);
                        pkt.ptsHns = sampleTime;
                        pkt.dtsHns = sampleTime;
                        pkt.durationHns = sampleDuration;
                        pkt.data.assign(pData, pData + currentLength);

                        if (m_callback) {
                            m_callback(std::move(pkt));
                        }

                        pMediaBuffer->Unlock();
                    }
                    pMediaBuffer->Release();
                }
            }
        }

        pSample->Release();
    }
#endif

} // namespace Recorder::Encode
