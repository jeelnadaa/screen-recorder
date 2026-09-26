#pragma once

#include "MuxTypes.h"
#include <string>
#include <future>
#include <functional>

namespace Recorder::Mux {

    class Mp4Remuxer {
    public:
        using CompletionCallback = std::function<void(const RemuxResult&)>;

        static RemuxResult RemuxSync(const std::wstring& mkvPath, const std::wstring& mp4Path, bool deleteSourceOnSuccess = true);
        static void RemuxAsync(const std::wstring& mkvPath, const std::wstring& mp4Path, bool deleteSourceOnSuccess, CompletionCallback callback);
    };

} // namespace Recorder::Mux
