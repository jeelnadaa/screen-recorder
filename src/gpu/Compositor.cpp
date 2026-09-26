#include "gpu/Compositor.h"

namespace Recorder::Gpu {

    Compositor::Compositor() = default;

    Compositor::~Compositor() {
#if defined(_WIN32)
        Cleanup();
#endif
    }

#if defined(_WIN32)
    bool Compositor::Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
        m_device = device;
        m_context = context;
        m_initialized = (m_device != nullptr && m_context != nullptr);
        return m_initialized;
    }

    void Compositor::Cleanup() {
        if (m_targetRtv) {
            m_targetRtv->Release();
            m_targetRtv = nullptr;
        }
        m_initialized = false;
    }

    bool Compositor::Compose(
        ID3D11Texture2D* sourceTexture,
        ID3D11Texture2D* targetTexture,
        const CompositionParams& params
    ) {
        if (!m_initialized || !sourceTexture || !targetTexture) return false;

        D3D11_TEXTURE2D_DESC srcDesc;
        sourceTexture->GetDesc(&srcDesc);

        D3D11_TEXTURE2D_DESC dstDesc;
        targetTexture->GetDesc(&dstDesc);

        if (params.hasCrop) {
            D3D11_BOX box;
            box.left = static_cast<UINT>(params.sourceCrop.left);
            box.top = static_cast<UINT>(params.sourceCrop.top);
            box.front = 0;
            box.right = static_cast<UINT>(params.sourceCrop.right);
            box.bottom = static_cast<UINT>(params.sourceCrop.bottom);
            box.back = 1;

            // Direct GPU copy of sub-region
            m_context->CopySubresourceRegion(targetTexture, 0, 0, 0, 0, sourceTexture, 0, &box);
        } else if (srcDesc.Width == dstDesc.Width && srcDesc.Height == dstDesc.Height && srcDesc.Format == dstDesc.Format) {
            // Full texture direct GPU copy
            m_context->CopyResource(targetTexture, sourceTexture);
        } else {
            // Resolution scaling fallback copy box
            D3D11_BOX box;
            box.left = 0;
            box.top = 0;
            box.front = 0;
            box.right = (srcDesc.Width < dstDesc.Width) ? srcDesc.Width : dstDesc.Width;
            box.bottom = (srcDesc.Height < dstDesc.Height) ? srcDesc.Height : dstDesc.Height;
            box.back = 1;

            m_context->CopySubresourceRegion(targetTexture, 0, 0, 0, 0, sourceTexture, 0, &box);
        }

        return true;
    }
#endif

} // namespace Recorder::Gpu
