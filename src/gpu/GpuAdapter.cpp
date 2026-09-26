#include "gpu/GpuAdapter.h"
#include <iostream>

#if defined(_WIN32)
#include <dxgi1_2.h>
#pragma comment(lib, "dxgi.lib")
#endif

namespace Recorder::Gpu {

    GpuVendor GpuAdapterManager::VendorFromId(uint32_t vendorId) {
        switch (vendorId) {
            case 0x10DE: return GpuVendor::Nvidia;
            case 0x8086: return GpuVendor::Intel;
            case 0x1002: return GpuVendor::Amd;
            case 0x5143: return GpuVendor::Qualcomm;
            default:     return GpuVendor::Other;
        }
    }

    std::wstring GpuAdapterManager::VendorToString(GpuVendor vendor) {
        switch (vendor) {
            case GpuVendor::Nvidia: return L"NVIDIA";
            case GpuVendor::Intel:  return L"Intel";
            case GpuVendor::Amd:    return L"AMD";
            case GpuVendor::Qualcomm: return L"Qualcomm";
            default:                return L"Generic GPU";
        }
    }

    std::vector<GpuDeviceInfo> GpuAdapterManager::EnumerateAdapters() {
        std::vector<GpuDeviceInfo> devices;

#if defined(_WIN32)
        IDXGIFactory1* pFactory = nullptr;
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&pFactory)))) {
            return devices;
        }

        UINT adapterIndex = 0;
        IDXGIAdapter1* pAdapter = nullptr;

        while (pFactory->EnumAdapters1(adapterIndex, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
            DXGI_ADAPTER_DESC1 desc;
            if (SUCCEEDED(pAdapter->GetDesc1(&desc))) {
                GpuDeviceInfo info;
                info.index = adapterIndex;
                info.name = desc.Description;
                info.vendorId = desc.VendorId;
                info.deviceId = desc.DeviceId;
                info.vendor = VendorFromId(desc.VendorId);
                info.dedicatedVramBytes = desc.DedicatedVideoMemory;
                info.isSoftwareAdapter = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;

                devices.push_back(info);
            }
            pAdapter->Release();
            adapterIndex++;
        }

        pFactory->Release();
#endif

        return devices;
    }

} // namespace Recorder::Gpu
