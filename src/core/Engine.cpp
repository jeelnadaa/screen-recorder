#include "core/Engine.h"
#include "config/Settings.h"
#include "config/PathTemplates.h"
#include <iostream>
#include <filesystem>

namespace Recorder::Core {

    Engine& Engine::Instance() {
        static Engine s_instance;
        return s_instance;
    }

    Engine::Engine() = default;

    Engine::~Engine() {
        Shutdown();
    }

    bool Engine::Initialize() {
        const auto& settings = Config::SettingsManager::Instance().Get();

        // 1. Initialize GPU subsystem
        if (!m_gpuDevice.Initialize(settings.video.adapterIndex)) {
            // Error: GPU initialization failed
            EventBus::Instance().Publish(ErrorEvent{ -1, L"GPU Error", L"Failed to initialize Direct3D 11 device on selected GPU adapter.", true });
            return false;
        }

#if defined(_WIN32)
        m_compositor.Initialize(m_gpuDevice.GetDevice(), m_gpuDevice.GetContext());
#endif

        // 2. Initialize HUD window
        m_hud.Create();

        // 3. Initialize Named Pipe Automation Server
        m_pipeServer.Start([this](const std::string& cmd) -> std::string {
            if (cmd == "START") {
                CaptureSourceDescriptor src;
                src.type = CaptureSourceType::Monitor;
                return StartRecording(src) ? "OK:STARTED\n" : "ERR:FAILED_TO_START\n";
            } else if (cmd == "STOP") {
                return StopRecording() ? "OK:STOPPED\n" : "ERR:FAILED_TO_STOP\n";
            } else if (cmd == "PAUSE") {
                return PauseRecording() ? "OK:PAUSED\n" : "ERR:FAILED_TO_PAUSE\n";
            } else if (cmd == "RESUME") {
                return ResumeRecording() ? "OK:RESUMED\n" : "ERR:FAILED_TO_RESUME\n";
            } else if (cmd == "REPLAY") {
                return SaveReplay() ? "OK:REPLAY_SAVED\n" : "ERR:NO_REPLAY_DATA\n";
            } else if (cmd == "STATUS") {
                return std::string("STATE:") + ToString(GetState()) + "\n";
            }
            return "ERR:UNKNOWN_COMMAND\n";
        });

        // 4. Start Telemetry and Watchdog thread
        m_workerRunning = true;
        m_watchdogThread = std::thread(&Engine::TelemetryWatchdogLoop, this);

        return true;
    }

    void Engine::Shutdown() {
        if (m_stateMachine.GetState() == EngineState::Recording || m_stateMachine.GetState() == EngineState::Paused) {
            StopRecording();
        }

        m_workerRunning = false;
        if (m_watchdogThread.joinable()) {
            m_watchdogThread.join();
        }

        m_pipeServer.Stop();
        m_hud.Destroy();
        m_gpuDevice.Cleanup();
    }

    bool Engine::StartRecording(const CaptureSourceDescriptor& source) {
        if (!m_stateMachine.TransitionTo(EngineState::Starting)) {
            return false;
        }

        const auto& settings = Config::SettingsManager::Instance().Get();
        m_activeSource = source;

        // Resolve destination file paths
        Config::TemplateContext ctx;
        ctx.appName = source.title.empty() ? L"Desktop" : source.title;
        ctx.width = settings.video.width;
        ctx.height = settings.video.height;
        ctx.fps = settings.video.targetFps;
        ctx.codec = (settings.video.codec == VideoCodec::HEVC) ? L"HEVC" : (settings.video.codec == VideoCodec::AV1 ? L"AV1" : L"H264");
        ctx.gpuVendor = m_gpuDevice.GetActiveGpu().name;

        std::wstring fileName = Config::PathTemplates::ResolveFileName(settings.output.filenameTemplate, ctx);
        m_currentMkvPath = Config::PathTemplates::CombinePath(settings.output.destinationDirectory, fileName, L"mkv");
        m_currentMp4Path = Config::PathTemplates::CombinePath(settings.output.destinationDirectory, fileName, L"mp4");

        // Ensure directory exists
        try {
            std::filesystem::create_directories(settings.output.destinationDirectory);
        } catch (...) {}

        // Open crash-resilient MKV Muxer
        Mux::MuxerConfig muxCfg;
        muxCfg.outputPath = m_currentMkvPath;
        muxCfg.video = settings.video;
        muxCfg.audio = settings.audio;
        if (!m_mkvMuxer.Open(muxCfg)) {
            m_stateMachine.TransitionTo(EngineState::Faulted);
            EventBus::Instance().Publish(ErrorEvent{ -2, L"Disk I/O Error", L"Could not open target MKV recording file for writing.", true });
            return false;
        }

        // Initialize Hardware MFT Video Encoder
        m_videoEncoder = std::make_unique<Encode::MftVideoEncoder>();
        m_videoEncoder->SetPacketCallback([this](MediaPacket&& pkt) {
            m_telemetry.RecordFrameEncoded(pkt.data.size());
            if (!m_packetQueue.TryPush(std::move(pkt))) {
                m_telemetry.RecordFrameDropped(L"Packet Queue Overflow (Disk I/O bottleneck)");
            }
        });

#if defined(_WIN32)
        auto encStatus = m_videoEncoder->Initialize(settings.video, m_gpuDevice.GetDevice(), m_gpuDevice.GetDxgiDeviceManager());
        if (encStatus == Encode::EncoderStatus::HardwareUnavailable) {
            m_mkvMuxer.Close();
            m_stateMachine.TransitionTo(EngineState::Faulted);
            EventBus::Instance().Publish(ErrorEvent{ -3, L"Hardware Encoding Unavailable",
                L"No hardware MFT encoder found on the selected GPU. The engine will not silently degrade to CPU encoding.", true });
            return false;
        } else if (encStatus != Encode::EncoderStatus::Ok) {
            m_mkvMuxer.Close();
            m_stateMachine.TransitionTo(EngineState::Faulted);
            EventBus::Instance().Publish(ErrorEvent{ -4, L"Encoder Error", L"Failed to configure Media Foundation encoder.", true });
            return false;
        }
#endif

        // Select and initialize Capture Engine (WGC primary, DXGI fallback)
        if (source.type == CaptureSourceType::Window || source.type == CaptureSourceType::Region) {
            m_captureEngine = std::make_unique<Capture::WgcEngine>();
        } else {
            // Monitor capture: use WGC or DXGI based on config
            m_captureEngine = std::make_unique<Capture::WgcEngine>();
        }

#if defined(_WIN32)
        m_captureEngine->Initialize(m_gpuDevice.GetDevice(), m_gpuDevice.GetContext());
#endif
        m_captureEngine->SetCursorCaptureEnabled(settings.overlays.showCursor);

        // Connect Capture Frame callback -> Hardware Encoder
        m_captureEngine->SetFrameCallback([this, &settings](
#if defined(_WIN32)
            ID3D11Texture2D* texture,
#else
            void* texture,
#endif
            int64_t timestampHns
        ) {
            if (m_stateMachine.GetState() != EngineState::Recording) {
                return; // Dropped or paused
            }

            m_telemetry.RecordFrameCaptured();

#if defined(_WIN32)
            if (m_videoEncoder) {
                if (!m_videoEncoder->SubmitFrame(texture, timestampHns)) {
                    m_telemetry.RecordFrameDropped(L"Hardware MFT rejected frame submission");
                }
            }
#endif
        });

        // Initialize Audio Subsystem
        m_audioEngine.Initialize(settings.audio);
        m_audioEngine.SetPacketCallback([this](MediaPacket&& pkt) {
            m_packetQueue.TryPush(std::move(pkt));
        });

        // Start Muxer disk writer thread
        m_muxerThread = std::thread(&Engine::MuxerThreadLoop, this);

        // Start capture and audio
        m_audioEngine.Start();
        if (!m_captureEngine->StartCapture(source)) {
            m_audioEngine.Stop();
            m_mkvMuxer.Close();
            m_stateMachine.TransitionTo(EngineState::Faulted);
            EventBus::Instance().Publish(ErrorEvent{ -5, L"Capture Error", L"Failed to start Windows capture session.", true });
            return false;
        }

        m_recordingStartTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        m_stateMachine.TransitionTo(EngineState::Recording);

        if (settings.overlays.showRecordingIndicator) {
            m_hud.Show();
        }

        return true;
    }

    bool Engine::PauseRecording() {
        if (!m_stateMachine.TransitionTo(EngineState::Paused)) {
            return false;
        }
        m_hud.Update(0, true);
        return true;
    }

    bool Engine::ResumeRecording() {
        if (!m_stateMachine.TransitionTo(EngineState::Recording)) {
            return false;
        }
        m_hud.Update(0, false);
        return true;
    }

    bool Engine::StopRecording() {
        if (!m_stateMachine.TransitionTo(EngineState::Stopping)) {
            return false;
        }

        m_hud.Hide();

        if (m_captureEngine) {
            m_captureEngine->StopCapture();
        }
        m_audioEngine.Stop();

        if (m_videoEncoder) {
            m_videoEncoder->Drain();
        }

        // Wait for muxer thread to process remaining queued packets
        if (m_muxerThread.joinable()) {
            m_muxerThread.join();
        }

        uint64_t finalSize = m_mkvMuxer.GetBytesWritten();
        uint64_t duration = m_mkvMuxer.GetDurationMs();
        m_mkvMuxer.Close();

        const auto& settings = Config::SettingsManager::Instance().Get();
        if (settings.output.autoRemuxMp4) {
            m_stateMachine.TransitionTo(EngineState::Remuxing);

            std::wstring mkv = m_currentMkvPath;
            std::wstring mp4 = m_currentMp4Path;
            bool deleteMkv = settings.output.deleteMkvAfterRemux;

            Mux::Mp4Remuxer::RemuxAsync(mkv, mp4, deleteMkv, [this, mkv, mp4, duration](const Mux::RemuxResult& res) {
                RecordingFinishedEvent evt;
                evt.mkvFilePath = mkv;
                evt.mp4FilePath = res.mp4Path;
                evt.remuxSuccess = res.success;
                evt.fileSizeBytes = res.fileSizeBytes;
                evt.durationMs = duration;

                EventBus::Instance().Publish(evt);
                m_stateMachine.ResetToIdle();
            });
        } else {
            RecordingFinishedEvent evt;
            evt.mkvFilePath = m_currentMkvPath;
            evt.mp4FilePath = L"";
            evt.remuxSuccess = false;
            evt.fileSizeBytes = finalSize;
            evt.durationMs = duration;

            EventBus::Instance().Publish(evt);
            m_stateMachine.ResetToIdle();
        }

        return true;
    }

    bool Engine::SaveReplay() {
        auto packets = m_replayBuffer.SnapshotPackets();
        if (packets.empty()) return false;

        const auto& settings = Config::SettingsManager::Instance().Get();
        Config::TemplateContext ctx;
        ctx.appName = L"Replay";
        std::wstring fileName = L"Replay_" + Config::PathTemplates::ResolveFileName(settings.output.filenameTemplate, ctx);
        std::wstring outMkv = Config::PathTemplates::CombinePath(settings.output.destinationDirectory, fileName, L"mkv");
        std::wstring outMp4 = Config::PathTemplates::CombinePath(settings.output.destinationDirectory, fileName, L"mp4");

        Mux::MkvMuxer replayMuxer;
        Mux::MuxerConfig cfg;
        cfg.outputPath = outMkv;
        cfg.video = settings.video;
        cfg.audio = settings.audio;

        if (replayMuxer.Open(cfg)) {
            for (const auto& pkt : packets) {
                replayMuxer.WritePacket(pkt);
            }
            uint64_t dur = replayMuxer.GetDurationMs();
            replayMuxer.Close();

            if (settings.output.autoRemuxMp4) {
                Mux::Mp4Remuxer::RemuxAsync(outMkv, outMp4, settings.output.deleteMkvAfterRemux, [dur, outMp4](const Mux::RemuxResult& res) {
                    ReplaySavedEvent evt;
                    evt.outputFilePath = res.mp4Path;
                    evt.durationMs = dur;
                    EventBus::Instance().Publish(evt);
                });
            } else {
                ReplaySavedEvent evt;
                evt.outputFilePath = outMkv;
                evt.durationMs = dur;
                EventBus::Instance().Publish(evt);
            }
            return true;
        }

        return false;
    }

    bool Engine::StartReplayBuffer() {
        const auto& settings = Config::SettingsManager::Instance().Get();
        m_replayBuffer.Configure(settings.replay.bufferDurationSeconds, settings.replay.maxRamMb);
        m_replayActive.store(true);
        return true;
    }

    void Engine::StopReplayBuffer() {
        m_replayActive.store(false);
        m_replayBuffer.Reset();
    }

    void Engine::SetMicMuted(bool muted) {
        m_audioEngine.SetMicMuted(muted);
    }

    bool Engine::IsMicMuted() const {
        return m_audioEngine.IsMicMuted();
    }

    void Engine::MuxerThreadLoop() {
        MediaPacket packet;
        while (m_stateMachine.GetState() == EngineState::Recording || 
               m_stateMachine.GetState() == EngineState::Paused ||
               !m_packetQueue.Empty()) {

            if (m_packetQueue.TryPop(packet)) {
                // 1. Write to active recording container
                m_mkvMuxer.WritePacket(packet);

                // 2. Feed into Instant Replay buffer if enabled
                if (m_replayActive.load()) {
                    MediaPacket copy = packet;
                    m_replayBuffer.PushPacket(std::move(copy));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
    }

    void Engine::TelemetryWatchdogLoop() {
        while (m_workerRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));

            const auto& settings = Config::SettingsManager::Instance().Get();
            m_telemetry.UpdateSystemMetrics(settings.output.destinationDirectory);

            auto snap = m_telemetry.GetSnapshot();

            // Update HUD elapsed time
            if (m_stateMachine.GetState() == EngineState::Recording) {
                auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                uint64_t elapsed = (nowMs > m_recordingStartTime) ? (nowMs - m_recordingStartTime) : 0;
                m_hud.Update(elapsed, false);
            }

            // Check low disk space threshold
            if (snap.freeDiskSpaceMb < settings.output.lowDiskWarningMb && snap.freeDiskSpaceMb > 0) {
                LowDiskSpaceEvent evt;
                evt.freeBytes = snap.freeDiskSpaceMb * 1024 * 1024;
                evt.thresholdBytes = static_cast<uint64_t>(settings.output.lowDiskWarningMb) * 1024 * 1024;
                EventBus::Instance().Publish(evt);
            }

            // Publish telemetry snapshot
            TelemetryUpdatedEvent tEvt;
            tEvt.telemetry = snap;
            EventBus::Instance().Publish(tEvt);
        }
    }

} // namespace Recorder::Core
