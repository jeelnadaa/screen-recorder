#include "gpu/D3D11Device.h"
#include <iostream>

#if defined(_WIN32)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "mfplat.lib")
#endif

namespace Recorder::Gpu {

    D3D11DeviceContextWrapper::D3D11DeviceContextWrapper() = default;

    D3D11DeviceContextWrapper::~D3D11DeviceContextWrapper() {
        Cleanup();
    }

    bool D3D11DeviceContextWrapper::IsInitialized() const {
        return m_initialized;
    }

    void D3D11DeviceContextWrapper::Cleanup() {
#if defined(_WIN32)
        if (m_dxgiDeviceManager) {
            m_dxgiDeviceManager->Release();
            m_dxgiDeviceManager = nullptr;
        }
        if (m_d3dContext) {
            m_d3dContext->ClearState();
            m_d3dContext->Flush();
            m_d3dContext->Release();
            m_d3dContext = nullptr;
        }
        if (m_d3dDevice) {
            m_d3dDevice->Release();
            m_d3dDevice = nullptr;
        }
#endif
        m_initialized = false;
    }

    bool D3D11DeviceContextWrapper::Initialize(uint32_t adapterIndex) {
        Cleanup();

#if defined(_WIN32)
        auto adapters = GpuAdapterManager::EnumerateAdapters();
        IDXGIAdapter* pSelectedAdapter = nullptr;

        IDXGIFactory1* pFactory = nullptr;
        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&pFactory)))) {
            IDXGIAdapter1* pAdapter1 = nullptr;
            if (SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &pAdapter1))) {
                pSelectedAdapter = pAdapter1;
                if (adapterIndex < adapters.size()) {
                    m_activeGpu = adapters[adapterIndex];
                }
            }
            pFactory->Release();
        }

        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;

        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1
        };
        D3D_FEATURE_LEVEL featureLevel;

        D3D_DRIVER_TYPE driverType = pSelectedAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

        HRESULT hr = D3D11CreateDevice(
            pSelectedAdapter,
            driverType,
            nullptr,
            creationFlags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &m_d3dDevice,
            &featureLevel,
            &m_d3dContext
        );

        if (pSelectedAdapter) {
            pSelectedAdapter->Release();
        }

        if (FAILED(hr)) {
            return false;
        }

        // Enable multithreading protection for D3D11 context
        ID3D11Multithread* pMultithread = nullptr;
        if (SUCCEEDED(m_d3dContext->QueryInterface(__uuidof(ID3D11Multithread), reinterpret_cast<void**>(&pMultithread)))) {
            pMultithread->SetMultithreadProtected(TRUE);
            pMultithread->Release();
        }

        // Initialize Media Foundation DXGI Device Manager for hardware MFT encoder sharing
        hr = MFCreateDXGIDeviceManager(&m_resetToken, &m_dxgiDeviceManager);
        if (SUCCEEDED(hr)) {
            m_dxgiDeviceManager->ResetDevice(m_d3dDevice, m_resetToken);
        }

        m_initialized = true;
        return true;
#else
        m_initialized = true;
        return true;
#endif
    }

#if defined(_WIN32)
    ID3D11Texture2D* D3D11DeviceContextWrapper::CreateTexture(uint32_t width, uint32_t height, DXGI_FORMAT format, bool isRenderTarget) {
        if (!m_d3dDevice) return nullptr;

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (isRenderTarget ? D3D11_BIND_RENDER_TARGET : 0);
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

        ID3D11Texture2D* texture = nullptr;
        HRESULT hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, &texture);
        return SUCCEEDED(hr) ? texture : nullptr;
    }
#endif

} // namespace Recorder::Gpu
