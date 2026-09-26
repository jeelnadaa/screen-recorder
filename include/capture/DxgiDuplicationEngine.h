#pragma once

#include "ICaptureEngine.h"
#include <atomic>
#include <thread>
#include <mutex>

#if defined(_WIN32)
#include <d3d11.h>
#include <dxgi1_2.h>
#endif

namespace Recorder::Capture {

    class DxgiDuplicationEngine : public ICaptureEngine {
    public:
        DxgiDuplicationEngine();
        ~DxgiDuplicationEngine() override;

#if defined(_WIN32)
        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
#endif
        bool StartCapture(const Core::CaptureSourceDescriptor& source) override;
        void StopCapture() override;
        bool IsCapturing() const override;

        void SetFrameCallback(FrameCallback callback) override;
        void SetCursorCaptureEnabled(bool enabled) override;
        void SetBorderRequired(bool required) override;
        void ExcludeWindow(void* hwnd) override;

        const char* GetEngineName() const override { return "DXGI Desktop Duplication"; }

    private:
        void CaptureLoop();

#if defined(_WIN32)
        ID3D11Device* m_d3dDevice = nullptr;
        ID3D11DeviceContext* m_d3dContext = nullptr;
        IDXGIOutputDuplication* m_deskDupl = nullptr;
#endif

        mutable std::mutex m_mutex;
        FrameCallback m_callback;
        std::atomic<bool> m_isCapturing{ false };
        std::thread m_captureThread;
        bool m_cursorCaptureEnabled = true;
    };

} // namespace Recorder::Capture
