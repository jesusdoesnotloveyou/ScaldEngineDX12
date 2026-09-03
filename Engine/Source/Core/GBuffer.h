#pragma once

#include "DXHelper.h"
#include "DescriptorHeap.h"
#include <array>

namespace Scald
{
    class Device;

    class GBuffer final
    {
    public:
        enum EGBufferLayer : UINT
        {
            DIFFUSE_ALBEDO = 0u,
            AMBIENT_OCCLUSION,
            NORMAL,
            SPECULAR,
            MOTION_VECTORS,
            DEPTH,  // Must be the last texture. Have to add some check.
            MAX = 6u
        };

    public:
        GBuffer(Device* device, UINT width, UINT height);
        GBuffer(const GBuffer& buffer) = delete;
        GBuffer& operator=(const GBuffer& buffer) = delete;

        ~GBuffer() noexcept;

    public:
        FORCEINLINE UINT GetWidth() const { return m_width; }
        FORCEINLINE UINT GetHeight() const { return m_height; }

        // if screen resized
        void OnResize(UINT newWidth, UINT newHeight);

        ID3D12Resource* Get(const unsigned layer) const;
        DXGI_FORMAT GetBufferTextureFormat(const unsigned layer) const;

        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(const unsigned layer = 0u) const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetRtv(const unsigned layer = 0u) const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const;
        
        void CreateDescriptors();

    private:
        void CreateViews();
        void CreateResources();

    private:
        Device* m_device = nullptr;

        UINT m_width, m_height;

        std::array<ComPtr<ID3D12Resource>, EGBufferLayer::MAX> m_textures = {};
        DescriptorHeapAllocation m_srvAllocation;  // Handles from GPU-visible descriptor heap
        DescriptorHeapAllocation m_dsvAllocation;  // Handle for depth layer
        DescriptorHeapAllocation m_rtvAllocation;  // Handles for other GBuffer layers
    };
}  // namespace Scald