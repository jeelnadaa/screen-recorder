#pragma once

#include "core/Types.h"
#include <cstdint>

#if defined(_WIN32)
#include <d3d11.h>
#endif

namespace Recorder::Gpu {

    struct CompositionParams {
        Core::Rect sourceCrop;
        uint32_t outputWidth = 1920;
        uint32_t outputHeight = 1080;
        bool hasCrop = false;
        bool showWatermark = false;
        bool showWebcam = false;
        Core::Rect webcamRect;
    };

    class Compositor {
    public:
        Compositor();
        ~Compositor();

#if defined(_WIN32)
        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
        void Cleanup();

        // Performs GPU-side copy / blit / crop directly between textures in VRAM
        bool Compose(
            ID3D11Texture2D* sourceTexture,
            ID3D11Texture2D* targetTexture,
            const CompositionParams& params
        );
#endif

    private:
#if defined(_WIN32)
        ID3D11Device* m_device = nullptr;
        ID3D11DeviceContext* m_context = nullptr;
        ID3D11RenderTargetView* m_targetRtv = nullptr;
#endif
        bool m_initialized = false;
    };

} // namespace Recorder::Gpu
