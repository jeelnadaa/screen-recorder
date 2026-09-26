#include "audio/MftAudioEncoder.h"
#include <iostream>

#if defined(_WIN32)
#include <wmcodecdsp.h>
#include <mferror.h>
#pragma comment(lib, "wmcodecdspuuid.lib")
#endif

namespace Recorder::Audio {

    MftAudioEncoder::MftAudioEncoder() = default;

    MftAudioEncoder::~MftAudioEncoder() {
        Shutdown();
    }

    void MftAudioEncoder::Shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
#if defined(_WIN32)
        if (m_transform) {
            m_transform->Release();
            m_transform = nullptr;
        }
#endif
        m_initialized = false;
    }

    bool MftAudioEncoder::Initialize(const Core::AudioConfig& config) {
        Shutdown();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = config;

#if defined(_WIN32)
        MFStartup(MF_VERSION);

        HRESULT hr = CoCreateInstance(CLSID_AACMFTEncoder, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IMFTransform), reinterpret_cast<void**>(&m_transform));
        if (FAILED(hr) || !m_transform) {
            return false;
        }

        // Set Output Media Type (AAC)
        IMFMediaType* pOutputType = nullptr;
        hr = MFCreateMediaType(&pOutputType);
        if (SUCCEEDED(hr)) {
            pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            pOutputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
            pOutputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, m_config.sampleRate);
            pOutputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, m_config.channels);
            pOutputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (m_config.bitrateKbps * 1000) / 8);
            pOutputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);

            hr = m_transform->SetOutputType(0, pOutputType, 0);
            pOutputType->Release();
        }

        // Set Input Media Type (PCM)
        IMFMediaType* pInputType = nullptr;
        hr = MFCreateMediaType(&pInputType);
        if (SUCCEEDED(hr)) {
            pInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            pInputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
            pInputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, m_config.sampleRate);
            pInputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, m_config.channels);
            pInputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
            pInputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, m_config.channels * 2);
            pInputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, m_config.sampleRate * m_config.channels * 2);

            hr = m_transform->SetInputType(0, pInputType, 0);
            pInputType->Release();
        }

        m_transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        m_transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

        m_initialized = true;
        return true;
#else
        m_initialized = true;
        return true;
#endif
    }

    bool MftAudioEncoder::Encode(const float* pcmData, size_t sampleCount, int64_t timestampHns, std::vector<Core::MediaPacket>& outPackets) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized || !pcmData || sampleCount == 0) return false;

#if defined(_WIN32)
        if (!m_transform) return false;

        // Convert float32 to int16 PCM
        std::vector<int16_t> pcm16(sampleCount);
        for (size_t i = 0; i < sampleCount; ++i) {
            float s = pcmData[i];
            if (s > 1.0f) s = 1.0f;
            else if (s < -1.0f) s = -1.0f;
            pcm16[i] = static_cast<int16_t>(s * 32767.0f);
        }

        IMFMediaBuffer* pBuffer = nullptr;
        DWORD bytesToProcess = static_cast<DWORD>(sampleCount * sizeof(int16_t));
        HRESULT hr = MFCreateMemoryBuffer(bytesToProcess, &pBuffer);
        if (FAILED(hr)) return false;

        BYTE* pDest = nullptr;
        if (SUCCEEDED(pBuffer->Lock(&pDest, nullptr, nullptr))) {
            memcpy(pDest, pcm16.data(), bytesToProcess);
            pBuffer->Unlock();
            pBuffer->SetCurrentLength(bytesToProcess);
        }

        IMFSample* pSample = nullptr;
        hr = MFCreateSample(&pSample);
        if (SUCCEEDED(hr)) {
            pSample->AddBuffer(pBuffer);
            pSample->SetSampleTime(timestampHns);
            pSample->SetSampleDuration((sampleCount * 10'000'000LL) / (m_config.sampleRate * m_config.channels));

            m_transform->ProcessInput(0, pSample, 0);
            pSample->Release();
        }
        pBuffer->Release();

        // Retrieve AAC frames
        MFT_OUTPUT_DATA_BUFFER outputDataBuffer = {};
        IMFSample* pOutSample = nullptr;
        MFCreateSample(&pOutSample);

        IMFMediaBuffer* pOutBuffer = nullptr;
        MFCreateMemoryBuffer(4096, &pOutBuffer);
        pOutSample->AddBuffer(pOutBuffer);
        pOutBuffer->Release();

        outputDataBuffer.pSample = pOutSample;
        DWORD status = 0;

        while (true) {
            hr = m_transform->ProcessOutput(0, 1, &outputDataBuffer, &status);
            if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT || FAILED(hr)) {
                break;
            }

            LONGLONG sampleTime = 0;
            LONGLONG sampleDuration = 0;
            outputDataBuffer.pSample->GetSampleTime(&sampleTime);
            outputDataBuffer.pSample->GetSampleDuration(&sampleDuration);

            IMFMediaBuffer* pContiguous = nullptr;
            if (SUCCEEDED(outputDataBuffer.pSample->ConvertToContiguousBuffer(&pContiguous))) {
                BYTE* pData = nullptr;
                DWORD length = 0;
                if (SUCCEEDED(pContiguous->Lock(&pData, nullptr, &length))) {
                    Core::MediaPacket pkt;
                    pkt.type = Core::PacketType::AudioFrame;
                    pkt.isKeyframe = false;
                    pkt.ptsHns = sampleTime;
                    pkt.dtsHns = sampleTime;
                    pkt.durationHns = sampleDuration;
                    pkt.data.assign(pData, pData + length);

                    outPackets.push_back(std::move(pkt));
                    pContiguous->Unlock();
                }
                pContiguous->Release();
            }
        }

        pOutSample->Release();
        return true;
#else
        return true;
#endif
    }

} // namespace Recorder::Audio
