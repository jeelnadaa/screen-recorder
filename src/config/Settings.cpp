#include "config/Settings.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#endif

namespace Recorder::Config {

    SettingsManager& SettingsManager::Instance() {
        static SettingsManager s_instance;
        return s_instance;
    }

    SettingsManager::SettingsManager() {
        SetDefaults();
    }

    UserSettings& SettingsManager::Get() {
        return m_settings;
    }

    const UserSettings& SettingsManager::Get() const {
        return m_settings;
    }

    std::wstring SettingsManager::GetDefaultRecordingsPath() const {
#if defined(_WIN32)
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, 0, nullptr, &path))) {
            std::wstring result(path);
            CoTaskMemFree(path);
            result += L"\\Captures";
            return result;
        }
#endif
        return L"D:\\Recordings";
    }

    std::wstring SettingsManager::GetDefaultConfigPath() const {
#if defined(_WIN32)
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
            std::wstring result(path);
            CoTaskMemFree(path);
            result += L"\\ScreenRecorder\\config.json";
            return result;
        }
#endif
        return L"config.json";
    }

    void SettingsManager::SetDefaults() {
        m_settings.version = 1;
        
        // Video Defaults
        m_settings.video.codec = Core::VideoCodec::H264;
        m_settings.video.bitrateMode = Core::BitrateMode::CBR;
        m_settings.video.rateControlMode = Core::RateControlMode::CFR;
        m_settings.video.width = 1920;
        m_settings.video.height = 1080;
        m_settings.video.targetFps = 60;
        m_settings.video.targetBitrateKbps = 30000;
        m_settings.video.maxBitrateKbps = 40000;
        m_settings.video.cqpQuality = 20;
        m_settings.video.gopSize = 120;
        m_settings.video.adapterIndex = 0;
        m_settings.video.hdrToneMapping = true;
        m_settings.video.allowSoftwareFallback = false;

        // Audio Defaults
        m_settings.audio.codec = Core::AudioCodec::AAC;
        m_settings.audio.sampleRate = 48000;
        m_settings.audio.channels = 2;
        m_settings.audio.bitrateKbps = 192;
        m_settings.audio.systemAudioEnabled = true;
        m_settings.audio.systemAudioVolume = 1.0f;
        m_settings.audio.micEnabled = true;
        m_settings.audio.micDeviceId = L"default";
        m_settings.audio.micVolume = 1.0f;
        m_settings.audio.perProcessAudioEnabled = false;
        m_settings.audio.targetProcessName = L"";

        // Output Defaults
        m_settings.output.destinationDirectory = GetDefaultRecordingsPath();
        m_settings.output.filenameTemplate = L"{date}_{time}_{app}_{res}_{fps}";
        m_settings.output.containerFormat = L"MKV";
        m_settings.output.autoRemuxMp4 = true;
        m_settings.output.deleteMkvAfterRemux = true;
        m_settings.output.autoSplitSizeMb = 0;
        m_settings.output.autoSplitDurationMin = 0;
        m_settings.output.lowDiskWarningMb = 5120;

        // Replay Defaults
        m_settings.replay.enabled = false;
        m_settings.replay.bufferDurationSeconds = 60;
        m_settings.replay.maxRamMb = 1024;

        // Hotkey Defaults
        m_settings.hotkeys.startStop = L"Ctrl+Shift+R";
        m_settings.hotkeys.pauseResume = L"Ctrl+Shift+P";
        m_settings.hotkeys.saveReplay = L"Ctrl+Shift+S";
        m_settings.hotkeys.screenshot = L"Ctrl+Shift+F12";
        m_settings.hotkeys.muteMic = L"Ctrl+Shift+M";
        m_settings.hotkeys.addMarker = L"Ctrl+Shift+B";

        // Overlay Defaults
        m_settings.overlays.showRecordingIndicator = true;
        m_settings.overlays.showPerformanceStats = true;
        m_settings.overlays.showCursor = true;
        m_settings.overlays.highlightClicks = false;
        m_settings.overlays.webcamPipEnabled = false;
        m_settings.overlays.webcamPosition = "bottom_right";
        m_settings.overlays.webcamScale = 0.2f;

        // Process Trigger Defaults
        m_settings.triggers.autoRecordProcess = L"";
        m_settings.triggers.stopOnProcessExit = true;
    }

    static std::string WStringToString(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), str.data(), size, nullptr, nullptr);
        return str;
    }

    static std::wstring StringToWString(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
        std::wstring wstr(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), wstr.data(), size);
        return wstr;
    }

    bool SettingsManager::SaveToFile(const std::wstring& filePath) const {
        std::wstring targetPath = filePath.empty() ? GetDefaultConfigPath() : filePath;
        
        try {
            std::filesystem::path p(targetPath);
            if (p.has_parent_path()) {
                std::filesystem::create_directories(p.parent_path());
            }

            std::ofstream file(targetPath);
            if (!file.is_open()) return false;

            // Clean formatted JSON
            file << "{\n";
            file << "  \"version\": " << m_settings.version << ",\n";
            file << "  \"video\": {\n";
            file << "    \"codec\": " << static_cast<int>(m_settings.video.codec) << ",\n";
            file << "    \"bitrateMode\": " << static_cast<int>(m_settings.video.bitrateMode) << ",\n";
            file << "    \"rateControlMode\": " << static_cast<int>(m_settings.video.rateControlMode) << ",\n";
            file << "    \"width\": " << m_settings.video.width << ",\n";
            file << "    \"height\": " << m_settings.video.height << ",\n";
            file << "    \"targetFps\": " << m_settings.video.targetFps << ",\n";
            file << "    \"targetBitrateKbps\": " << m_settings.video.targetBitrateKbps << ",\n";
            file << "    \"maxBitrateKbps\": " << m_settings.video.maxBitrateKbps << ",\n";
            file << "    \"cqpQuality\": " << m_settings.video.cqpQuality << ",\n";
            file << "    \"adapterIndex\": " << m_settings.video.adapterIndex << ",\n";
            file << "    \"hdrToneMapping\": " << (m_settings.video.hdrToneMapping ? "true" : "false") << ",\n";
            file << "    \"allowSoftwareFallback\": " << (m_settings.video.allowSoftwareFallback ? "true" : "false") << "\n";
            file << "  },\n";
            file << "  \"audio\": {\n";
            file << "    \"codec\": " << static_cast<int>(m_settings.audio.codec) << ",\n";
            file << "    \"sampleRate\": " << m_settings.audio.sampleRate << ",\n";
            file << "    \"channels\": " << m_settings.audio.channels << ",\n";
            file << "    \"bitrateKbps\": " << m_settings.audio.bitrateKbps << ",\n";
            file << "    \"systemAudioEnabled\": " << (m_settings.audio.systemAudioEnabled ? "true" : "false") << ",\n";
            file << "    \"systemAudioVolume\": " << m_settings.audio.systemAudioVolume << ",\n";
            file << "    \"micEnabled\": " << (m_settings.audio.micEnabled ? "true" : "false") << ",\n";
            file << "    \"micVolume\": " << m_settings.audio.micVolume << "\n";
            file << "  },\n";
            file << "  \"output\": {\n";
            file << "    \"destinationDirectory\": \"" << WStringToString(m_settings.output.destinationDirectory) << "\",\n";
            file << "    \"filenameTemplate\": \"" << WStringToString(m_settings.output.filenameTemplate) << "\",\n";
            file << "    \"autoRemuxMp4\": " << (m_settings.output.autoRemuxMp4 ? "true" : "false") << ",\n";
            file << "    \"deleteMkvAfterRemux\": " << (m_settings.output.deleteMkvAfterRemux ? "true" : "false") << ",\n";
            file << "    \"lowDiskWarningMb\": " << m_settings.output.lowDiskWarningMb << "\n";
            file << "  },\n";
            file << "  \"replay\": {\n";
            file << "    \"enabled\": " << (m_settings.replay.enabled ? "true" : "false") << ",\n";
            file << "    \"bufferDurationSeconds\": " << m_settings.replay.bufferDurationSeconds << ",\n";
            file << "    \"maxRamMb\": " << m_settings.replay.maxRamMb << "\n";
            file << "  },\n";
            file << "  \"hotkeys\": {\n";
            file << "    \"startStop\": \"" << WStringToString(m_settings.hotkeys.startStop) << "\",\n";
            file << "    \"pauseResume\": \"" << WStringToString(m_settings.hotkeys.pauseResume) << "\",\n";
            file << "    \"saveReplay\": \"" << WStringToString(m_settings.hotkeys.saveReplay) << "\",\n";
            file << "    \"screenshot\": \"" << WStringToString(m_settings.hotkeys.screenshot) << "\",\n";
            file << "    \"muteMic\": \"" << WStringToString(m_settings.hotkeys.muteMic) << "\",\n";
            file << "    \"addMarker\": \"" << WStringToString(m_settings.hotkeys.addMarker) << "\"\n";
            file << "  }\n";
            file << "}\n";

            return true;
        } catch (...) {
            return false;
        }
    }

    bool SettingsManager::LoadFromFile(const std::wstring& filePath) {
        std::wstring targetPath = filePath.empty() ? GetDefaultConfigPath() : filePath;
        
        try {
            if (!std::filesystem::exists(targetPath)) {
                return false;
            }

            std::ifstream file(targetPath);
            if (!file.is_open()) return false;

            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            
            auto getIntValue = [&](const std::string& key, int defaultVal) -> int {
                size_t pos = content.find("\"" + key + "\"");
                if (pos == std::string::npos) return defaultVal;
                size_t colon = content.find(':', pos);
                if (colon == std::string::npos) return defaultVal;
                size_t start = content.find_first_of("0123456789-", colon);
                if (start == std::string::npos) return defaultVal;
                size_t end = content.find_first_of(",\n\r}", start);
                return std::stoi(content.substr(start, end - start));
            };

            auto getBoolValue = [&](const std::string& key, bool defaultVal) -> bool {
                size_t pos = content.find("\"" + key + "\"");
                if (pos == std::string::npos) return defaultVal;
                size_t colon = content.find(':', pos);
                if (colon == std::string::npos) return defaultVal;
                size_t start = content.find_first_not_of(" \t\r\n", colon + 1);
                if (start == std::string::npos) return defaultVal;
                if (content.compare(start, 4, "true") == 0) return true;
                if (content.compare(start, 5, "false") == 0) return false;
                return defaultVal;
            };

            auto getStringValue = [&](const std::string& key, const std::wstring& defaultVal) -> std::wstring {
                size_t pos = content.find("\"" + key + "\"");
                if (pos == std::string::npos) return defaultVal;
                size_t colon = content.find(':', pos);
                if (colon == std::string::npos) return defaultVal;
                size_t quoteStart = content.find('"', colon);
                if (quoteStart == std::string::npos) return defaultVal;
                size_t quoteEnd = content.find('"', quoteStart + 1);
                if (quoteEnd == std::string::npos) return defaultVal;
                return StringToWString(content.substr(quoteStart + 1, quoteEnd - quoteStart - 1));
            };

            m_settings.video.width = (uint32_t)getIntValue("width", 1920);
            m_settings.video.height = (uint32_t)getIntValue("height", 1080);
            m_settings.video.targetFps = (uint32_t)getIntValue("targetFps", 60);
            m_settings.video.targetBitrateKbps = (uint32_t)getIntValue("targetBitrateKbps", 30000);
            m_settings.video.cqpQuality = (uint32_t)getIntValue("cqpQuality", 20);
            m_settings.video.codec = static_cast<Core::VideoCodec>(getIntValue("codec", 0));
            m_settings.video.bitrateMode = static_cast<Core::BitrateMode>(getIntValue("bitrateMode", 0));
            m_settings.video.rateControlMode = static_cast<Core::RateControlMode>(getIntValue("rateControlMode", 0));
            m_settings.video.adapterIndex = (uint32_t)getIntValue("adapterIndex", 0);
            m_settings.video.hdrToneMapping = getBoolValue("hdrToneMapping", true);
            m_settings.video.allowSoftwareFallback = getBoolValue("allowSoftwareFallback", false);

            m_settings.output.destinationDirectory = getStringValue("destinationDirectory", GetDefaultRecordingsPath());
            m_settings.output.filenameTemplate = getStringValue("filenameTemplate", L"{date}_{time}_{app}_{res}_{fps}");
            m_settings.output.autoRemuxMp4 = getBoolValue("autoRemuxMp4", true);
            m_settings.output.deleteMkvAfterRemux = getBoolValue("deleteMkvAfterRemux", true);
            m_settings.output.lowDiskWarningMb = (uint32_t)getIntValue("lowDiskWarningMb", 5120);

            m_settings.replay.enabled = getBoolValue("enabled", false);
            m_settings.replay.bufferDurationSeconds = (uint32_t)getIntValue("bufferDurationSeconds", 60);

            m_settings.hotkeys.startStop = getStringValue("startStop", L"Ctrl+Shift+R");
            m_settings.hotkeys.pauseResume = getStringValue("pauseResume", L"Ctrl+Shift+P");
            m_settings.hotkeys.saveReplay = getStringValue("saveReplay", L"Ctrl+Shift+S");

            return true;
        } catch (...) {
            return false;
        }
    }

} // namespace Recorder::Config
