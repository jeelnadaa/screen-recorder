#pragma once

#include "../core/Types.h"
#include <functional>
#include <string>

#if defined(_WIN32)
#include <d3d11.h>
#endif

namespace Recorder::Capture {

    using FrameCallback = std::function<void(
#if defined(_WIN32)
        ID3D11Texture2D* texture,
#else
        void* texture,
#endif
        int64_t timestampHns
    )>;

    class ICaptureEngine {
    public:
        virtual ~ICaptureEngine() = default;

#if defined(_WIN32)
        virtual bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) = 0;
#endif
        virtual bool StartCapture(const Core::CaptureSourceDescriptor& source) = 0;
        virtual void StopCapture() = 0;
        virtual bool IsCapturing() const = 0;

        virtual void SetFrameCallback(FrameCallback callback) = 0;
        virtual void SetCursorCaptureEnabled(bool enabled) = 0;
        virtual void SetBorderRequired(bool required) = 0;
        virtual void ExcludeWindow(void* hwnd) = 0;

        virtual const char* GetEngineName() const = 0;
    };

} // namespace Recorder::Capture
