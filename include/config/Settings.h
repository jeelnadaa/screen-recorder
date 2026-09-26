#pragma once

#include "../core/Types.h"
#include <string>

namespace Recorder::Config {

    struct OutputSettings {
        std::wstring destinationDirectory = L"";
        std::wstring filenameTemplate = L"{date}_{time}_{app}_{res}_{fps}";
        std::wstring containerFormat = L"MKV";
        bool autoRemuxMp4 = true;
        bool deleteMkvAfterRemux = true;
        uint32_t autoSplitSizeMb = 0;
        uint32_t autoSplitDurationMin = 0;
        uint32_t lowDiskWarningMb = 5120; // 5 GB
    };

    struct ReplaySettings {
        bool enabled = false;
        uint32_t bufferDurationSeconds = 60;
        uint32_t maxRamMb = 1024;
    };

    struct HotkeySettings {
        std::wstring startStop = L"Ctrl+Shift+R";
        std::wstring pauseResume = L"Ctrl+Shift+P";
        std::wstring saveReplay = L"Ctrl+Shift+S";
        std::wstring screenshot = L"Ctrl+Shift+F12";
        std::wstring muteMic = L"Ctrl+Shift+M";
        std::wstring addMarker = L"Ctrl+Shift+B";
    };

    struct OverlaySettings {
        bool showRecordingIndicator = true;
        bool showPerformanceStats = true;
        bool showCursor = true;
        bool highlightClicks = false;
        bool webcamPipEnabled = false;
        std::wstring webcamDeviceId = L"";
        std::string webcamPosition = "bottom_right"; // "top_left", "top_right", "bottom_left", "bottom_right"
        float webcamScale = 0.2f;
    };

    struct ProcessTriggerSettings {
        std::wstring autoRecordProcess = L"";
        bool stopOnProcessExit = true;
    };

    struct UserSettings {
        uint32_t version = 1;
        Core::VideoConfig video;
        Core::AudioConfig audio;
        OutputSettings output;
        ReplaySettings replay;
        HotkeySettings hotkeys;
        OverlaySettings overlays;
        ProcessTriggerSettings triggers;
    };

    class SettingsManager {
    public:
        static SettingsManager& Instance();

        UserSettings& Get();
        const UserSettings& Get() const;

        bool LoadFromFile(const std::wstring& filePath = L"");
        bool SaveToFile(const std::wstring& filePath = L"") const;
        void SetDefaults();

        std::wstring GetDefaultConfigPath() const;
        std::wstring GetDefaultRecordingsPath() const;

    private:
        SettingsManager();
        ~SettingsManager() = default;

        UserSettings m_settings;
    };

} // namespace Recorder::Config
