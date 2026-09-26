#pragma once

#include "../core/Types.h"
#include <vector>
#include <string>

namespace Recorder::Capture {

    class SourceManager {
    public:
        static std::vector<Core::CaptureSourceDescriptor> EnumerateMonitors();
        static std::vector<Core::CaptureSourceDescriptor> EnumerateWindows();
        static Core::CaptureSourceDescriptor GetActiveWindowSource();
        static Core::CaptureSourceDescriptor CreateRegionSource(const Core::CaptureSourceDescriptor& baseSource, const Core::Rect& region);

        static bool IsProcessRunning(const std::wstring& processName);
    };

} // namespace Recorder::Capture
