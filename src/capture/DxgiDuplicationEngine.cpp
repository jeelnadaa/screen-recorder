#include "capture/DxgiDuplicationEngine.h"
#include <iostream>
#include <chrono>

#if defined(_WIN32)
#include <avrt.h>
#pragma comment(lib, "avrt.lib")
#endif

namespace Recorder::Capture {

    DxgiDuplicationEngine::DxgiDuplicationEngine() = default;

    DxgiDuplicationEngine::~DxgiDuplicationEngine() {
        StopCapture();
    }

#if defined(_WIN32)
    bool DxgiDuplicationEngine::Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_d3dDevice = device;
        m_d3dContext = context;
        return (m_d3dDevice != nullptr);
    }
#endif

    bool DxgiDuplicationEngine::StartCapture(const Core::CaptureSourceDescriptor& /*source*/) {
#if defined(_WIN32)
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_isCapturing || !m_d3dDevice) return false;

        // Obtain IDXGIDevice from D3D11
        IDXGIDevice* dxgiDevice = nullptr;
        if (FAILED(m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice)))) {
            return false;
        }

        IDXGIAdapter* adapter = nullptr;
        if (FAILED(dxgiDevice->GetAdapter(&adapter))) {
            dxgiDevice->Release();
            return false;
        }
        dxgiDevice->Release();

        // Enumerate primary output
        IDXGIOutput* output = nullptr;
        if (FAILED(adapter->EnumOutputs(0, &output))) {
            adapter->Release();
            return false;
        }
        adapter->Release();

        IDXGIOutput1* output1 = nullptr;
        if (FAILED(output->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&output1)))) {
            output->Release();
            return false;
        }
        output->Release();

        HRESULT hr = output1->DuplicateOutput(m_d3dDevice, &m_deskDupl);
        output1->Release();

        if (FAILED(hr) || !m_deskDupl) {
            return false;
        }

        m_isCapturing = true;
        m_captureThread = std::thread(&DxgiDuplicationEngine::CaptureLoop, this);
        return true;
#else
        m_isCapturing = true;
        return true;
#endif
    }

    void DxgiDuplicationEngine::StopCapture() {
        if (!m_isCapturing.exchange(false)) return;

        if (m_captureThread.joinable()) {
            m_captureThread.join();
        }

#if defined(_WIN32)
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_deskDupl) {
            m_deskDupl->Release();
            m_deskDupl = nullptr;
        }
#endif
    }

    bool DxgiDuplicationEngine::IsCapturing() const {
        return m_isCapturing.load();
    }

    void DxgiDuplicationEngine::SetFrameCallback(FrameCallback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callback = std::move(callback);
    }

    void DxgiDuplicationEngine::SetCursorCaptureEnabled(bool enabled) {
        m_cursorCaptureEnabled = enabled;
    }

    void DxgiDuplicationEngine::SetBorderRequired(bool /*required*/) {
        // DXGI does not draw a border
    }

    void DxgiDuplicationEngine::ExcludeWindow(void* /*hwnd*/) {
        // Window exclusion is handled by DWM / SetWindowDisplayAffinity
    }

    void DxgiDuplicationEngine::CaptureLoop() {
#if defined(_WIN32)
        // Elevate capture pump to Multimedia Class Scheduler Service (MMCSS)
        DWORD taskIndex = 0;
        HANDLE hMmcss = AvSetMmThreadCharacteristicsW(L"DisplayPostProcessing", &taskIndex);

        while (m_isCapturing) {
            DXGI_OUTDUPL_FRAME_INFO frameInfo{};
            IDXGIResource* desktopResource = nullptr;

            HRESULT hr = m_deskDupl->AcquireNextFrame(30, &frameInfo, &desktopResource);
            if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
                continue;
            }

            if (FAILED(hr)) {
                // If device lost or desktop switch occurs, break cleanly
                break;
            }

            ID3D11Texture2D* texture = nullptr;
            hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture));
            desktopResource->Release();

            if (SUCCEEDED(hr) && texture) {
                LARGE_INTEGER qpc;
                QueryPerformanceCounter(&qpc);
                int64_t timestampHns = qpc.QuadPart;

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

            m_deskDupl->ReleaseFrame();
        }

        if (hMmcss) {
            AvRevertMmThreadCharacteristics(hMmcss);
        }
#endif
    }

} // namespace Recorder::Capture
