#include "encode/HardwareDetector.h"
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#include <mfapi.h>
#include <mftransform.h>
#include <mfidl.h>
#pragma comment(lib, "mfplat.lib")
#endif

namespace Recorder::Encode {

    EncoderCapabilities HardwareDetector::DetectCapabilities() {
        EncoderCapabilities caps;

#if defined(_WIN32)
        MFStartup(MF_VERSION);

        auto checkCodec = [](const GUID& subtype, std::wstring& outName) -> bool {
            MFT_REGISTER_TYPE_INFO outputType = { MFMediaType_Video, subtype };
            IMFActivate** ppActivate = nullptr;
            UINT32 count = 0;

            HRESULT hr = MFTEnumEx(
                MFT_CATEGORY_VIDEO_ENCODER,
                MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
                nullptr,
                &outputType,
                &ppActivate,
                &count
            );

            if (SUCCEEDED(hr) && count > 0 && ppActivate) {
                WCHAR* name = nullptr;
                UINT32 nameLen = 0;
                if (SUCCEEDED(ppActivate[0]->GetAllocatedString(MFT_FRIENDLY_NAME_Attribute, &name, &nameLen))) {
                    outName = name;
                    CoTaskMemFree(name);
                }

                for (UINT32 i = 0; i < count; ++i) {
                    ppActivate[i]->Release();
                }
                CoTaskMemFree(ppActivate);
                return true;
            }
            return false;
        };

        std::wstring h264Name, hevcName, av1Name;
        caps.hasHardwareH264 = checkCodec(MFVideoFormat_H264, h264Name);
        caps.hasHardwareHEVC = checkCodec(MFVideoFormat_HEVC, hevcName);
        caps.hasHardwareAV1 = checkCodec(MFVideoFormat_AV1, av1Name);

        if (!h264Name.empty()) {
            caps.primaryEncoderDescription = h264Name;
            if (h264Name.find(L"NVIDIA") != std::wstring::npos) {
                caps.hardwareVendorName = L"NVIDIA NVENC";
            } else if (h264Name.find(L"Intel") != std::wstring::npos) {
                caps.hardwareVendorName = L"Intel QuickSync";
            } else if (h264Name.find(L"AMD") != std::wstring::npos) {
                caps.hardwareVendorName = L"AMD AMF";
            } else {
                caps.hardwareVendorName = L"Hardware MFT";
            }
        }

        MFShutdown();
#else
        caps.hasHardwareH264 = true;
        caps.hasHardwareHEVC = true;
        caps.hasHardwareAV1 = false;
        caps.hardwareVendorName = L"Mock Hardware";
#endif

        return caps;
    }

    bool HardwareDetector::HasHardwareEncoder(Core::VideoCodec codec) {
        auto caps = DetectCapabilities();
        switch (codec) {
            case Core::VideoCodec::H264: return caps.hasHardwareH264;
            case Core::VideoCodec::HEVC: return caps.hasHardwareHEVC;
            case Core::VideoCodec::AV1:  return caps.hasHardwareAV1;
            default: return false;
        }
    }

} // namespace Recorder::Encode
