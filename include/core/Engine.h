#pragma once

#include "Types.h"
#include "StateMachine.h"
#include "LockFreeQueue.h"
#include "EventBus.h"
#include "gpu/D3D11Device.h"
#include "gpu/Compositor.h"
#include "capture/ICaptureEngine.h"
#include "capture/WgcEngine.h"
#include "capture/DxgiDuplicationEngine.h"
#include "encode/IVideoEncoder.h"
#include "encode/MftVideoEncoder.h"
#include "audio/WasapiAudioEngine.h"
#include "mux/MkvMuxer.h"
#include "mux/Mp4Remuxer.h"
#include "replay/CircularBuffer.h"
#include "overlay/RecordingHud.h"
#include "overlay/Telemetry.h"
#include "ipc/NamedPipeServer.h"

#include <memory>
#include <thread>
#include <atomic>

namespace Recorder::Core {

    class Engine {
    public:
        static Engine& Instance();

        bool Initialize();
        void Shutdown();

        // Control API
        bool StartRecording(const CaptureSourceDescriptor& source);
        bool PauseRecording();
        bool ResumeRecording();
        bool StopRecording();
        bool SaveReplay();

        // Replay buffer mode
        bool StartReplayBuffer();
        void StopReplayBuffer();
        bool IsReplayBufferActive() const { return m_replayActive.load(); }

        // Source & status queries
        EngineState GetState() const { return m_stateMachine.GetState(); }
        PerformanceTelemetry GetTelemetry() const { return m_telemetry.GetSnapshot(); }
        std::vector<Overlay::DroppedFrameRecord> GetDroppedFrameLogs() const { return m_telemetry.GetDroppedFrameLogs(); }

        void SetMicMuted(bool muted);
        bool IsMicMuted() const;

    private:
        Engine();
        ~Engine();

        void MuxerThreadLoop();
        void TelemetryWatchdogLoop();

        StateMachine m_stateMachine;
        Gpu::D3D11DeviceContextWrapper m_gpuDevice;
        Gpu::Compositor m_compositor;

        std::unique_ptr<Capture::ICaptureEngine> m_captureEngine;
        std::unique_ptr<Encode::IVideoEncoder> m_videoEncoder;
        Audio::WasapiAudioEngine m_audioEngine;
        Mux::MkvMuxer m_mkvMuxer;
        Replay::CircularBuffer m_replayBuffer;
        Overlay::RecordingHud m_hud;
        Overlay::TelemetryCollector m_telemetry;
        Ipc::NamedPipeServer m_pipeServer;

        // Packet queue between encoder threads and disk muxer thread
        LockFreeQueue<MediaPacket, 4096> m_packetQueue;

        std::thread m_muxerThread;
        std::thread m_watchdogThread;
        std::atomic<bool> m_workerRunning{ false };
        std::atomic<bool> m_replayActive{ false };

        CaptureSourceDescriptor m_activeSource;
        std::wstring m_currentMkvPath;
        std::wstring m_currentMp4Path;
        int64_t m_recordingStartTime = 0;
    };

} // namespace Recorder::Core
