#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

namespace Recorder::Core {

    enum class EngineState {
        Idle,
        Starting,
        Recording,
        Paused,
        Stopping,
        Remuxing,
        Faulted
    };

    inline const char* ToString(EngineState state) {
        switch (state) {
            case EngineState::Idle: return "Idle";
            case EngineState::Starting: return "Starting";
            case EngineState::Recording: return "Recording";
            case EngineState::Paused: return "Paused";
            case EngineState::Stopping: return "Stopping";
            case EngineState::Remuxing: return "Remuxing";
            case EngineState::Faulted: return "Faulted";
            default: return "Unknown";
        }
    }

    inline const wchar_t* ToWString(EngineState state) {
        switch (state) {
            case EngineState::Idle: return L"Idle";
            case EngineState::Starting: return L"Starting";
            case EngineState::Recording: return L"Recording";
            case EngineState::Paused: return L"Paused";
            case EngineState::Stopping: return L"Stopping";
            case EngineState::Remuxing: return L"Remuxing";
            case EngineState::Faulted: return L"Faulted";
            default: return L"Unknown";
        }
    }

    enum class CaptureSourceType {
        Monitor,
        Window,
        Process,
        Region
    };

    struct Rect {
        int32_t left = 0;
        int32_t top = 0;
        int32_t right = 0;
        int32_t bottom = 0;

        int32_t Width() const { return right - left; }
        int32_t Height() const { return bottom - top; }
    };

    struct CaptureSourceDescriptor {
        CaptureSourceType type = CaptureSourceType::Monitor;
        std::wstring id;            // Monitor handle ID or Window handle ID string
        std::wstring title;         // Display name or window title
        std::wstring processName;   // e.g. "Game.exe"
        void* nativeHandle = nullptr; // HMONITOR or HWND
        Rect region;                // Active region when type == Region
        int32_t nativeWidth = 1920;
        int32_t nativeHeight = 1080;
        bool isPrimaryMonitor = false;
    };

    enum class VideoCodec {
        H264,
        HEVC,
        AV1
    };

    enum class BitrateMode {
        CBR,
        VBR,
        CQP
    };

    enum class RateControlMode {
        CFR, // Constant Frame Rate
        VFR  // Variable Frame Rate
    };

    struct VideoConfig {
        VideoCodec codec = VideoCodec::H264;
        BitrateMode bitrateMode = BitrateMode::CBR;
        RateControlMode rateControlMode = RateControlMode::CFR;
        uint32_t width = 1920;
        uint32_t height = 1080;
        uint32_t targetFps = 60;
        uint32_t targetBitrateKbps = 30000;
        uint32_t maxBitrateKbps = 40000;
        uint32_t cqpQuality = 20;
        uint32_t gopSize = 120; // Keyframe interval (frames)
        uint32_t adapterIndex = 0; // GPU selector
        bool hdrToneMapping = true;
        bool allowSoftwareFallback = false;
    };

    enum class AudioCodec {
        AAC,
        Opus
    };

    struct AudioConfig {
        AudioCodec codec = AudioCodec::AAC;
        uint32_t sampleRate = 48000;
        uint32_t channels = 2;
        uint32_t bitrateKbps = 192;
        bool systemAudioEnabled = true;
        float systemAudioVolume = 1.0f;
        bool micEnabled = true;
        std::wstring micDeviceId = L"default";
        float micVolume = 1.0f;
        bool perProcessAudioEnabled = false;
        std::wstring targetProcessName;
    };

    enum class PacketType {
        VideoKeyframe,
        VideoDeltaFrame,
        AudioFrame
    };

    struct MediaPacket {
        PacketType type = PacketType::VideoDeltaFrame;
        int64_t ptsHns = 0; // Presentation timestamp in 100-nanosecond units (MF time)
        int64_t dtsHns = 0; // Decode timestamp
        int64_t durationHns = 0;
        bool isKeyframe = false;
        std::vector<uint8_t> data;
    };

    struct PerformanceTelemetry {
        double recorderCpuPercent = 0.0;
        double gpuEncodePercent = 0.0;
        double currentBitrateKbps = 0.0;
        uint64_t totalFramesCaptured = 0;
        uint64_t totalFramesEncoded = 0;
        uint64_t droppedFrames = 0;
        uint64_t recordingDurationMs = 0;
        uint64_t bytesWritten = 0;
        uint64_t freeDiskSpaceMb = 0;
    };

} // namespace Recorder::Core
