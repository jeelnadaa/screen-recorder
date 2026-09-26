#pragma once

#include <string>
#include <vector>
#include <cstdint>

#if defined(_WIN32)
#include <d3d11.h>
#include <dxgi.h>
#endif

namespace Recorder::Gpu {

    enum class GpuVendor {
        Nvidia,
        Intel,
        Amd,
        Qualcomm,
        Other
    };

    struct GpuDeviceInfo {
        uint32_t index = 0;
        std::wstring name;
        uint32_t vendorId = 0;
        uint32_t deviceId = 0;
        GpuVendor vendor = GpuVendor::Other;
        uint64_t dedicatedVramBytes = 0;
        bool isSoftwareAdapter = false;
    };

    class GpuAdapterManager {
    public:
        static std::vector<GpuDeviceInfo> EnumerateAdapters();
        static GpuVendor VendorFromId(uint32_t vendorId);
        static std::wstring VendorToString(GpuVendor vendor);
    };

} // namespace Recorder::Gpu
