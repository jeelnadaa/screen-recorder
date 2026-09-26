#pragma once

#include "ICaptureEngine.h"
#include <atomic>
#include <mutex>

#if defined(_WIN32)
#include <d3d11.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#endif

namespace Recorder::Capture {

    class WgcEngine : public ICaptureEngine {
    public:
        WgcEngine();
        ~WgcEngine() override;

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

        const char* GetEngineName() const override { return "Windows.Graphics.Capture (WGC)"; }

    private:
#if defined(_WIN32)
        ID3D11Device* m_d3dDevice = nullptr;
        ID3D11DeviceContext* m_d3dContext = nullptr;
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_winrtDevice{ nullptr };

        winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item{ nullptr };
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
        winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };
        winrt::event_token m_frameArrivedToken{};

        void OnFrameArrived(
            winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
            winrt::Windows::Foundation::IInspectable const& args
        );
#endif

        mutable std::mutex m_mutex;
        FrameCallback m_callback;
        std::atomic<bool> m_isCapturing{ false };
        bool m_cursorCaptureEnabled = true;
        bool m_borderRequired = false;
    };

} // namespace Recorder::Capture
