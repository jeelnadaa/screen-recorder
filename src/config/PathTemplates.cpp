#include "config/PathTemplates.h"
#include <iomanip>
#include <sstream>
#include <ctime>
#include <algorithm>

namespace Recorder::Config {

    std::wstring PathTemplates::SanitizeFileName(const std::wstring& fileName) {
        std::wstring sanitized = fileName;
        const std::wstring invalidChars = L"\\/:*?\"<>|";
        for (wchar_t& ch : sanitized) {
            if (invalidChars.find(ch) != std::wstring::npos || ch < 32) {
                ch = L'_';
            }
        }
        return sanitized;
    }

    std::wstring PathTemplates::ResolveFileName(const std::wstring& templateStr, const TemplateContext& context) {
        std::time_t tt = std::chrono::system_clock::to_time_t(context.timestamp);
        std::tm tmVal;
#if defined(_WIN32)
        localtime_s(&tmVal, &tt);
#else
        localtime_r(&tt, &tmVal);
#endif

        wchar_t dateBuf[32];
        swprintf_s(dateBuf, L"%04d-%02d-%02d", tmVal.tm_year + 1900, tmVal.tm_mon + 1, tmVal.tm_mday);

        wchar_t timeBuf[32];
        swprintf_s(timeBuf, L"%02d-%02d-%02d", tmVal.tm_hour, tmVal.tm_min, tmVal.tm_sec);

        std::wstring result = templateStr;

        auto replaceAll = [&](const std::wstring& token, const std::wstring& val) {
            size_t pos = 0;
            while ((pos = result.find(token, pos)) != std::wstring::npos) {
                result.replace(pos, token.length(), val);
                pos += val.length();
            }
        };

        replaceAll(L"{date}", dateBuf);
        replaceAll(L"{time}", timeBuf);
        replaceAll(L"{timestamp}", std::to_wstring(tt));
        replaceAll(L"{app}", SanitizeFileName(context.appName));
        replaceAll(L"{res}", std::to_wstring(context.width) + L"x" + std::to_wstring(context.height));
        replaceAll(L"{fps}", std::to_wstring(context.fps) + L"fps");
        replaceAll(L"{codec}", context.codec);
        replaceAll(L"{gpu}", SanitizeFileName(context.gpuVendor));

        return SanitizeFileName(result);
    }

    std::wstring PathTemplates::CombinePath(const std::wstring& dir, const std::wstring& fileName, const std::wstring& extension) {
        std::wstring path = dir;
        if (!path.empty() && path.back() != L'\\' && path.back() != L'/') {
            path += L'\\';
        }
        path += fileName;
        if (!extension.empty()) {
            if (extension.front() != L'.') {
                path += L'.';
            }
            path += extension;
        }
        return path;
    }

} // namespace Recorder::Config
