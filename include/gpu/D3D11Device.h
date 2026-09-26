#pragma once

#include <cstdint>
#include <memory>
#include "GpuAdapter.h"

#if defined(_WIN32)
#include <d3d11.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#endif

namespace Recorder::Gpu {

    class D3D11DeviceContextWrapper {
    public:
        D3D11DeviceContextWrapper();
        ~D3D11DeviceContextWrapper();

        bool Initialize(uint32_t adapterIndex = 0);
        void Cleanup();

        bool IsInitialized() const;

#if defined(_WIN32)
        ID3D11Device* GetDevice() const { return m_d3dDevice; }
        ID3D11DeviceContext* GetContext() const { return m_d3dContext; }
        IMFDXGIDeviceManager* GetDxgiDeviceManager() const { return m_dxgiDeviceManager; }
        UINT GetDeviceResetToken() const { return m_resetToken; }

        // Create texture helper
        ID3D11Texture2D* CreateTexture(uint32_t width, uint32_t height, DXGI_FORMAT format, bool isRenderTarget = false);
#endif

        const GpuDeviceInfo& GetActiveGpu() const { return m_activeGpu; }

    private:
#if defined(_WIN32)
        ID3D11Device* m_d3dDevice = nullptr;
        ID3D11DeviceContext* m_d3dContext = nullptr;
        IMFDXGIDeviceManager* m_dxgiDeviceManager = nullptr;
        UINT m_resetToken = 0;
#endif
        GpuDeviceInfo m_activeGpu;
        bool m_initialized = false;
    };

} // namespace Recorder::Gpu
