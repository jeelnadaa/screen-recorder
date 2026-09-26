#pragma once

#include "Settings.h"
#include <string>
#include <vector>

namespace Recorder::Config {

    struct PresetInfo {
        std::wstring id;
        std::wstring name;
        std::wstring description;
        UserSettings settings;
    };

    class PresetManager {
    public:
        static PresetManager& Instance();

        std::vector<PresetInfo> GetAvailablePresets() const;
        bool ApplyPreset(const std::wstring& presetId);
        bool SaveCustomPreset(const std::wstring& presetId, const std::wstring& name, const std::wstring& desc, const UserSettings& settings);

    private:
        PresetManager();
        void LoadBuiltInPresets();

        std::vector<PresetInfo> m_presets;
    };

} // namespace Recorder::Config
