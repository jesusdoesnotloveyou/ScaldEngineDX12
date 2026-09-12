#pragma once

#include "DXHelper.h"
#include "DescriptorHeap.h"
#include <array>

namespace Scald
{
    class Device;

    struct FrameResource;

    class SSAO
    {
    public:
        /// <summary>
        /// I gonna use depth and normal textures from G-Buffer pass, so I suppose there is no need
        /// to create and store these resources (as well as their descriptors) in SSAO class
        /// </summary>
        enum ESSAOTextureType
        {
            AmbientMap0 = 0u,
            AmbientMap1,
            RandomVectorsMap,
            MAX
        };

    public:
        SSAO(Device* device, ID3D12GraphicsCommandList* pCommandList, UINT width, UINT height);
        SSAO(const SSAO& ssao) = delete;
        SSAO(SSAO&& ssao) noexcept = delete;
        SSAO& operator=(const SSAO& ssao) = delete;
        SSAO& operator=(SSAO&& ssao) noexcept = delete;

        ~SSAO() noexcept = default;

        static const int MaxBlurRadius = 5;

    public:
        static DXGI_FORMAT GetAmbientMapFormat();

        void OnResize(UINT newWidth, UINT newHeight);

        FORCEINLINE D3D12_VIEWPORT GetViewport() const { return m_viewport; }
        FORCEINLINE D3D12_RECT GetScissorRect() const { return m_scissorRect; }

        FORCEINLINE UINT GetWidth() const { return m_renderTargetWidth / 2; }
        FORCEINLINE UINT GetHeight() const { return m_renderTargetHeight / 2; }

        ID3D12Resource* GetAmbientMap();

        void GetOffsetVectors(XMFLOAT4 offsets[14]);

        std::array<float, 2*MaxBlurRadius+1> CalcGaussWeights(float sigma);

        void CreateDescriptors();

        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuSrv() const;

        void SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso);

    private:
        void CreateResources();
        void CreateViews();

        void BuildRandomVectorTexture(ID3D12GraphicsCommandList* pCommandList);
        void BuildOffsetVectors();

    #pragma region Render
    public:
        void Compute(ID3D12GraphicsCommandList* pCommandList, FrameResource* currFrameResource, int blurPassesCount);
    private:
        ///< summary>
        /// Blurs the ambient map to smooth out the noise caused by only taking a
        /// few random samples per pixel.  We use an edge preserving blur so that
        /// we do not blur across discontinuities--we want edges to remain edges.
        ///</summary>
        void BlurAmbientMap(ID3D12GraphicsCommandList* pCommandList, FrameResource* currFrameResource, int blurCount);
        void BlurAmbientMap(ID3D12GraphicsCommandList* pCommandList, bool horzBlur);
    #pragma endregion Render

    private:
        Device* m_device = nullptr;

        UINT m_renderTargetWidth;
        UINT m_renderTargetHeight;

        ID3D12PipelineState* m_ssaoPso = nullptr;
        ID3D12PipelineState* m_ssaoBlurPso = nullptr;

        D3D12_VIEWPORT m_viewport;
        D3D12_RECT m_scissorRect;

        ComPtr<ID3D12Resource> m_randomVectorMapUploadBuffer;

        XMFLOAT4 m_offsets[14];

        std::array<ComPtr<ID3D12Resource>, ESSAOTextureType::MAX> m_textures;
        DescriptorHeapAllocation m_srvAllocation = {};  // Handles from GPU-visible descriptor heap
        DescriptorHeapAllocation m_rtvAllocation = {};  // Handles from RTV heap for binding as render target
    };
}  // namespace Scald