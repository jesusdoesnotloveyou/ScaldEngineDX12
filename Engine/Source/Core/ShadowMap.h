#pragma once

#include "DXHelper.h"
#include "DescriptorHeap.h"

namespace Scald
{
    class Device;

    class ShadowMap
    {
    public:
        ShadowMap(Device* device, UINT width, UINT height, UINT cascadesCount = 0u);
        ShadowMap(const ShadowMap& lhs) = delete;
        ShadowMap& operator=(const ShadowMap& lhs) = delete;

        virtual ~ShadowMap() noexcept;

    public:
        FORCEINLINE UINT GetWidth() const { return m_mapWidth; }
        FORCEINLINE UINT GetHeight() const { return m_mapHeight; }

        ID3D12Resource* Get();

        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuSrv() const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const;

        FORCEINLINE D3D12_VIEWPORT GetViewport() const { return m_viewport; }
        FORCEINLINE D3D12_RECT GetScissorRect() const { return m_scissorRect; }

        // TODO: method must be rewrited since I've changed descriptor allocation and managment strategy
        virtual void CreateDescriptors();

        void OnResize(UINT newWidth, UINT newHeight);

        FORCEINLINE float GetCascadeLevel(UINT level) const { return m_shadowCascadeLevels[level]; }

        // could be updatable if we are changing frustum in runtime
        void CreateShadowCascadeSplits(float nearZ, float farZ);

    protected:
        virtual void CreateViews();

    private:
        // TODO: Method must be rewritten since it is repeated in derived class CascadeShadowMap.
        void CreateResource();

    protected:
        Device* m_device = nullptr;

        D3D12_VIEWPORT m_viewport;
        D3D12_RECT m_scissorRect;

        DescriptorHeapAllocation m_srvAllocation;
        DescriptorHeapAllocation m_dsvAllocation;

        DXGI_FORMAT m_format = DXGI_FORMAT_R24G8_TYPELESS;

        UINT m_mapWidth = 0u;
        UINT m_mapHeight = 0u;

        UINT m_cascadesCount = 0u;

        // actual gpu resource
        ComPtr<ID3D12Resource> m_shadowMap = nullptr;

        float m_shadowCascadeLevels[MaxCascades] = {0.0f, 0.0f, 0.0f, 0.0f};
    };
}  // namespace Scald