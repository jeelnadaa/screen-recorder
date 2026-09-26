#include "capture/WgcEngine.h"
#include <iostream>

#if defined(_WIN32)
#include <windows.graphics.directx.direct3d11.interop.h>
#include <dwmapi.h>
#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "dwmapi.lib")
#endif

namespace Recorder::Capture {

    WgcEngine::WgcEngine() = default;

    WgcEngine::~WgcEngine() {
        StopCapture();
    }

#if defined(_WIN32)
    bool WgcEngine::Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_d3dDevice = device;
        m_d3dContext = context;

        if (!m_d3dDevice) return false;

        // Create WinRT Direct3D Device from D3D11 DXGI Device
        IDXGIDevice* dxgiDevice = nullptr;
        HRESULT hr = m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
        if (FAILED(hr)) return false;

        winrt::com_ptr<::IInspectable> inspectableDevice;
        hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice, inspectableDevice.put());
        dxgiDevice->Release();

        if (FAILED(hr)) return false;

        m_winrtDevice = inspectableDevice.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
        return true;
    }
#endif

    bool WgcEngine::StartCapture(const Core::CaptureSourceDescriptor& source) {
#if defined(_WIN32)
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_isCapturing || !m_winrtDevice) return false;

        auto interop = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        if (!interop) return false;

        HRESULT hr = E_FAIL;
        if (source.type == Core::CaptureSourceType::Window && source.nativeHandle) {
            hr = interop->CreateForWindow(reinterpret_cast<HWND>(source.nativeHandle), winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(m_item));
        } else if (source.type == Core::CaptureSourceType::Monitor && source.nativeHandle) {
            hr = interop->CreateForMonitor(reinterpret_cast<HMONITOR>(source.nativeHandle), winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(m_item));
        } else {
            // Default to primary monitor if handle is null
            POINT ptZero = { 0, 0 };
            HMONITOR hMon = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
            hr = interop->CreateForMonitor(hMon, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(m_item));
        }

        if (FAILED(hr) || !m_item) return false;

        auto itemSize = m_item.Size();
        m_framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
            m_winrtDevice,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            itemSize
        );

        m_frameArrivedToken = m_framePool.FrameArrived({ this, &WgcEngine::OnFrameArrived });

        m_session = m_framePool.CreateCaptureSession(m_item);
        
        // Configure options (Cursor visibility and Windows 11 yellow border suppression)
        try {
            m_session.IsCursorCaptureEnabled(m_cursorCaptureEnabled);
            m_session.IsBorderRequired(m_borderRequired);
        } catch (...) {
            // Older Windows 10 versions may not support IsBorderRequired; ignore safely
        }

        m_session.StartCapture();
        m_isCapturing = true;
        return true;
#else
        m_isCapturing = true;
        return true;
#endif
    }

    void WgcEngine::StopCapture() {
#if defined(_WIN32)
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_isCapturing) return;

        m_isCapturing = false;

        if (m_session) {
            m_session.Close();
            m_session = nullptr;
        }

        if (m_framePool) {
            m_framePool.FrameArrived(m_frameArrivedToken);
            m_framePool.Close();
            m_framePool = nullptr;
        }

        m_item = nullptr;
#else
        m_isCapturing = false;
#endif
    }

    bool WgcEngine::IsCapturing() const {
        return m_isCapturing.load();
    }

    void WgcEngine::SetFrameCallback(FrameCallback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callback = std::move(callback);
    }

    void WgcEngine::SetCursorCaptureEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cursorCaptureEnabled = enabled;
#if defined(_WIN32)
        if (m_session) {
            try {
                m_session.IsCursorCaptureEnabled(enabled);
            } catch (...) {}
        }
#endif
    }

    void WgcEngine::SetBorderRequired(bool required) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_borderRequired = required;
#if defined(_WIN32)
        if (m_session) {
            try {
                m_session.IsBorderRequired(required);
            } catch (...) {}
        }
#endif
    }

    void WgcEngine::ExcludeWindow(void* hwnd) {
#if defined(_WIN32)
        if (hwnd) {
            SetWindowDisplayAffinity(reinterpret_cast<HWND>(hwnd), WDA_EXCLUDEFROMCAPTURE);
        }
#endif
    }

#if defined(_WIN32)
    void WgcEngine::OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const&
    ) {
        if (!m_isCapturing) return;

        auto frame = sender.TryGetNextFrame();
        if (!frame) return;

        auto surface = frame.Surface();
        auto access = surface.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        if (!access) return;

        ID3D11Texture2D* texture = nullptr;
        HRESULT hr = access->GetInterface(IID_PPV_ARGS(&texture));
        if (SUCCEEDED(hr) && texture) {
            int64_t timestampHns = frame.SystemRelativeTime().count();

            FrameCallback cb;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                cb = m_callback;
            }

            if (cb) {
                cb(texture, timestampHns);
            }

            texture->Release();
        }
    }
#endif

} // namespace Recorder::Capture
