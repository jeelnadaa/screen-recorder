#include "config/PathTemplates.h"
#include "config/Settings.h"
#include "config/PresetManager.h"
#include "core/StateMachine.h"
#include "replay/CircularBuffer.h"
#include "mux/MkvMuxer.h"
#include <iostream>
#include <cassert>
#include <filesystem>

void TestPathTemplates() {
    std::cout << "[RUNNING] TestPathTemplates..." << std::endl;

    Recorder::Config::TemplateContext ctx;
    ctx.appName = L"Cyberpunk: 2077 / Special Edition";
    ctx.width = 2560;
    ctx.height = 1440;
    ctx.fps = 144;
    ctx.codec = L"HEVC";
    ctx.gpuVendor = L"NVIDIA RTX 4090";

    std::wstring tpl = L"{app}_{res}_{fps}_{codec}";
    std::wstring result = Recorder::Config::PathTemplates::ResolveFileName(tpl, ctx);

    // Verify invalid characters ':' and '/' were sanitized to '_'
    assert(result.find(L":") == std::wstring::npos);
    assert(result.find(L"/") == std::wstring::npos);
    assert(result.find(L"2560x1440") != std::wstring::npos);
    assert(result.find(L"144fps") != std::wstring::npos);
    assert(result.find(L"HEVC") != std::wstring::npos);

    std::wstring fullPath = Recorder::Config::PathTemplates::CombinePath(L"D:\\Captures", result, L"mkv");
    assert(fullPath.find(L".mkv") != std::wstring::npos);
    assert(fullPath.rfind(L"D:\\Captures\\", 0) == 0);

    std::cout << "  -> Resolved: " << std::string(result.begin(), result.end()) << std::endl;
    std::cout << "[PASSED] TestPathTemplates" << std::endl;
}

void TestSettingsSerialization() {
    std::cout << "[RUNNING] TestSettingsSerialization..." << std::endl;

    auto& sm = Recorder::Config::SettingsManager::Instance();
    sm.SetDefaults();

    auto& cfg = sm.Get();
    cfg.video.width = 3840;
    cfg.video.height = 2160;
    cfg.video.targetFps = 120;
    cfg.video.targetBitrateKbps = 60000;
    cfg.video.codec = Recorder::Core::VideoCodec::AV1;

    std::wstring testFile = L"test_config.json";
    bool saved = sm.SaveToFile(testFile);
    assert(saved);

    // Reset and reload
    sm.SetDefaults();
    assert(sm.Get().video.width == 1920);

    bool loaded = sm.LoadFromFile(testFile);
    assert(loaded);
    assert(sm.Get().video.width == 3840);
    assert(sm.Get().video.height == 2160);
    assert(sm.Get().video.targetFps == 120);
    assert(sm.Get().video.codec == Recorder::Core::VideoCodec::AV1);

    std::filesystem::remove(testFile);
    std::cout << "[PASSED] TestSettingsSerialization" << std::endl;
}

void TestPresets() {
    std::cout << "[RUNNING] TestPresets..." << std::endl;

    auto& pm = Recorder::Config::PresetManager::Instance();
    auto presets = pm.GetAvailablePresets();
    assert(presets.size() >= 3);

    bool foundGaming = false;
    for (const auto& p : presets) {
        if (p.id == L"gaming_low_overhead") {
            foundGaming = true;
            assert(p.settings.video.bitrateMode == Recorder::Core::BitrateMode::CQP);
            assert(!p.settings.video.allowSoftwareFallback);
        }
    }
    assert(foundGaming);

    bool applied = pm.ApplyPreset(L"esports_high_fps");
    assert(applied);
    assert(Recorder::Config::SettingsManager::Instance().Get().video.targetFps == 120);

    std::cout << "[PASSED] TestPresets" << std::endl;
}

void TestStateMachine() {
    std::cout << "[RUNNING] TestStateMachine..." << std::endl;

    Recorder::Core::StateMachine sm;
    assert(sm.GetState() == Recorder::Core::EngineState::Idle);

    // Valid lifecycle flow
    assert(sm.TransitionTo(Recorder::Core::EngineState::Starting));
    assert(sm.GetState() == Recorder::Core::EngineState::Starting);

    // Invalid transition
    assert(!sm.TransitionTo(Recorder::Core::EngineState::Paused));

    assert(sm.TransitionTo(Recorder::Core::EngineState::Recording));
    assert(sm.TransitionTo(Recorder::Core::EngineState::Paused));
    assert(sm.TransitionTo(Recorder::Core::EngineState::Recording));
    assert(sm.TransitionTo(Recorder::Core::EngineState::Stopping));
    assert(sm.TransitionTo(Recorder::Core::EngineState::Remuxing));
    assert(sm.TransitionTo(Recorder::Core::EngineState::Idle));

    std::cout << "[PASSED] TestStateMachine" << std::endl;
}

void TestCircularReplayBuffer() {
    std::cout << "[RUNNING] TestCircularReplayBuffer..." << std::endl;

    Recorder::Replay::CircularBuffer buffer;
    buffer.Configure(2, 64); // 2 seconds window

    // Push keyframe
    Recorder::Core::MediaPacket kf;
    kf.type = Recorder::Core::PacketType::VideoKeyframe;
    kf.isKeyframe = true;
    kf.ptsHns = 0;
    kf.data = { 0x00, 0x00, 0x00, 0x01, 0x67 }; // NAL header
    buffer.PushPacket(std::move(kf));

    // Push 60 delta frames
    for (int i = 1; i <= 60; ++i) {
        Recorder::Core::MediaPacket df;
        df.type = Recorder::Core::PacketType::VideoDeltaFrame;
        df.isKeyframe = false;
        df.ptsHns = i * 166666LL; // ~16.6ms intervals
        df.data = { 0x00, 0x00, 0x00, 0x01, 0x41, static_cast<uint8_t>(i) };
        buffer.PushPacket(std::move(df));
    }

    assert(buffer.GetPacketCount() == 61);
    auto snapshot = buffer.SnapshotPackets();
    assert(!snapshot.empty());
    assert(snapshot.front().isKeyframe); // First packet must be keyframe!

    std::cout << "  -> Replay buffer duration: " << buffer.GetBufferedDurationMs() << " ms" << std::endl;
    std::cout << "[PASSED] TestCircularReplayBuffer" << std::endl;
}

void TestMkvMuxer() {
    std::cout << "[RUNNING] TestMkvMuxer..." << std::endl;

    Recorder::Mux::MkvMuxer muxer;
    Recorder::Mux::MuxerConfig cfg;
    cfg.outputPath = L"test_output.mkv";
    cfg.video.width = 1920;
    cfg.video.height = 1080;
    cfg.video.codec = Recorder::Core::VideoCodec::H264;

    bool opened = muxer.Open(cfg);
    assert(opened);
    assert(muxer.IsOpen());

    // Write Keyframe
    Recorder::Core::MediaPacket kf;
    kf.type = Recorder::Core::PacketType::VideoKeyframe;
    kf.isKeyframe = true;
    kf.ptsHns = 10000000; // 1.0s
    kf.data = { 0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84 };
    bool written = muxer.WritePacket(kf);
    assert(written);

    // Write Delta frame
    Recorder::Core::MediaPacket df;
    df.type = Recorder::Core::PacketType::VideoDeltaFrame;
    df.isKeyframe = false;
    df.ptsHns = 10166666;
    df.data = { 0x00, 0x00, 0x00, 0x01, 0x41 };
    written = muxer.WritePacket(df);
    assert(written);

    assert(muxer.GetBytesWritten() > 0);
    muxer.Close();

    assert(std::filesystem::exists("test_output.mkv"));
    assert(std::filesystem::file_size("test_output.mkv") > 50);

    std::filesystem::remove("test_output.mkv");
    std::cout << "[PASSED] TestMkvMuxer" << std::endl;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "Running Screen Recorder Automated Tests..." << std::endl;
    std::cout << "==========================================" << std::endl;

    TestPathTemplates();
    TestSettingsSerialization();
    TestPresets();
    TestStateMachine();
    TestCircularReplayBuffer();
    TestMkvMuxer();

    std::cout << "==========================================" << std::endl;
    std::cout << "ALL 6 TEST SUITES PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "==========================================" << std::endl;
    return 0;
}
