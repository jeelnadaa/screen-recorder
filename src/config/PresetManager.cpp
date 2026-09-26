#include "config/PresetManager.h"

namespace Recorder::Config {

    PresetManager& PresetManager::Instance() {
        static PresetManager s_instance;
        return s_instance;
    }

    PresetManager::PresetManager() {
        LoadBuiltInPresets();
    }

    void PresetManager::LoadBuiltInPresets() {
        m_presets.clear();

        // 1. Gaming - Low Overhead (Primary recommended preset)
        {
            PresetInfo preset;
            preset.id = L"gaming_low_overhead";
            preset.name = L"Gaming — Low Overhead";
            preset.description = L"Minimal GPU/CPU impact. Uses CQP hardware encoding and MKV container with auto MP4 remux.";
            
            preset.settings.video.codec = Core::VideoCodec::H264;
            preset.settings.video.bitrateMode = Core::BitrateMode::CQP;
            preset.settings.video.rateControlMode = Core::RateControlMode::CFR;
            preset.settings.video.cqpQuality = 20;
            preset.settings.video.targetFps = 60;
            preset.settings.video.width = 1920;
            preset.settings.video.height = 1080;
            preset.settings.video.allowSoftwareFallback = false;

            preset.settings.audio.systemAudioEnabled = true;
            preset.settings.audio.micEnabled = true;
            preset.settings.audio.sampleRate = 48000;
            preset.settings.audio.bitrateKbps = 192;

            preset.settings.output.autoRemuxMp4 = true;
            preset.settings.output.deleteMkvAfterRemux = true;

            m_presets.push_back(preset);
        }

        // 2. Tutorial - High Quality
        {
            PresetInfo preset;
            preset.id = L"tutorial_high_quality";
            preset.name = L"Tutorial — High Quality";
            preset.description = L"Optimized for desktop clarity, text sharpness, and cursor/click highlighting.";
            
            preset.settings.video.codec = Core::VideoCodec::H264;
            preset.settings.video.bitrateMode = Core::BitrateMode::CBR;
            preset.settings.video.rateControlMode = Core::RateControlMode::CFR;
            preset.settings.video.targetBitrateKbps = 25000;
            preset.settings.video.targetFps = 60;
            preset.settings.video.width = 1920;
            preset.settings.video.height = 1080;

            preset.settings.overlays.showCursor = true;
            preset.settings.overlays.highlightClicks = true;

            m_presets.push_back(preset);
        }

        // 3. High-Framerate Esports (120/144 FPS)
        {
            PresetInfo preset;
            preset.id = L"esports_high_fps";
            preset.name = L"Esports — 120/144 FPS";
            preset.description = L"High temporal resolution recording with HEVC/AV1 to minimize PCIe bus and disk overhead.";
            
            preset.settings.video.codec = Core::VideoCodec::HEVC;
            preset.settings.video.bitrateMode = Core::BitrateMode::VBR;
            preset.settings.video.rateControlMode = Core::RateControlMode::CFR;
            preset.settings.video.targetBitrateKbps = 45000;
            preset.settings.video.maxBitrateKbps = 60000;
            preset.settings.video.targetFps = 120;
            preset.settings.video.width = 1920;
            preset.settings.video.height = 1080;

            m_presets.push_back(preset);
        }

        // 4. Compact Archive (Storage Saver)
        {
            PresetInfo preset;
            preset.id = L"archive_compact";
            preset.name = L"Compact Archive";
            preset.description = L"Space-saving 30 FPS HEVC encoding with CQP for long recordings and meeting archives.";
            
            preset.settings.video.codec = Core::VideoCodec::HEVC;
            preset.settings.video.bitrateMode = Core::BitrateMode::CQP;
            preset.settings.video.rateControlMode = Core::RateControlMode::CFR;
            preset.settings.video.cqpQuality = 24;
            preset.settings.video.targetFps = 30;
            preset.settings.video.width = 1920;
            preset.settings.video.height = 1080;

            m_presets.push_back(preset);
        }
    }

    std::vector<PresetInfo> PresetManager::GetAvailablePresets() const {
        return m_presets;
    }

    bool PresetManager::ApplyPreset(const std::wstring& presetId) {
        for (const auto& preset : m_presets) {
            if (preset.id == presetId) {
                SettingsManager::Instance().Get() = preset.settings;
                return true;
            }
        }
        return false;
    }

    bool PresetManager::SaveCustomPreset(const std::wstring& presetId, const std::wstring& name, const std::wstring& desc, const UserSettings& settings) {
        for (auto& preset : m_presets) {
            if (preset.id == presetId) {
                preset.name = name;
                preset.description = desc;
                preset.settings = settings;
                return true;
            }
        }

        PresetInfo custom;
        custom.id = presetId;
        custom.name = name;
        custom.description = desc;
        custom.settings = settings;
        m_presets.push_back(custom);
        return true;
    }

} // namespace Recorder::Config
