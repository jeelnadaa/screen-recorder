#pragma once

#include <string>
#include <chrono>

namespace Recorder::Config {

    struct TemplateContext {
        std::wstring appName = L"Desktop";
        uint32_t width = 1920;
        uint32_t height = 1080;
        uint32_t fps = 60;
        std::wstring codec = L"H264";
        std::wstring gpuVendor = L"GPU";
        std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    };

    class PathTemplates {
    public:
        static std::wstring ResolveFileName(const std::wstring& templateStr, const TemplateContext& context);
        static std::wstring SanitizeFileName(const std::wstring& fileName);
        static std::wstring CombinePath(const std::wstring& dir, const std::wstring& fileName, const std::wstring& extension);
    };

} // namespace Recorder::Config
