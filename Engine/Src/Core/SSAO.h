#pragma once

#include "Common/DXHelper.h"

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
		AmbientMap0,
		AmbientMap1,
		RandomVectors,
		Max
	};

	struct FSSAOTexture
	{
		CD3DX12_GPU_DESCRIPTOR_HANDLE m_hGpuSrv = {};
		CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuSrv = {};
		CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuRtv = {};
	};

public:
	SSAO(ID3D12Device* device, ID3D12GraphicsCommandList* pCommandList, UINT width, UINT height);
	SSAO(const SSAO& ssao) = delete;
	SSAO& operator=(const SSAO& ssao) = delete;

	~SSAO() noexcept = default;

	static const DXGI_FORMAT AmbientMapFormat = DXGI_FORMAT_R16_UNORM;

public:
	void OnResize(UINT newWidth, UINT newHeight);

	FORCEINLINE D3D12_VIEWPORT GetViewport() const { return m_viewport; }
	FORCEINLINE D3D12_RECT GetScissorRect() const { return m_scissorRect; }

	FORCEINLINE UINT GetWidth()const { return m_renderTargetWidth / 2; }
	FORCEINLINE UINT GetHeight()const { return m_renderTargetHeight / 2; }

	void GetOffsetVectors(XMFLOAT4 offsets[14]);

	void RebuildDescriptors(ID3D12Resource* depthStencilBuffer /* should be descriptor on existing resoucre */);

private:
	void BuildResources();

	void BuildRandomVectorTexture(ID3D12GraphicsCommandList* pCommandList);
	void BuildOffsetVectors();

#pragma region Render

	void ComputeSSAO(ID3D12GraphicsCommandList* pCommandList, FrameResource* frameResource, int blurPassesCount);

	///<summary>
	/// Blurs the ambient map to smooth out the noise caused by only taking a
	/// few random samples per pixel.  We use an edge preserving blur so that 
	/// we do not blur across discontinuities--we want edges to remain edges.
	///</summary>
	void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, FrameResource* currFrame, int blurCount);
	void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horzBlur);

#pragma endregion Render

private:
	ID3D12Device* m_device = nullptr;

	UINT m_renderTargetWidth;
	UINT m_renderTargetHeight;

	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;
	
	ComPtr<ID3D12Resource> m_randomVectorMap;
	ComPtr<ID3D12Resource> m_randomVectorMapUploadBuffer;
	ComPtr<ID3D12Resource> m_ambientMap0;
	ComPtr<ID3D12Resource> m_ambientMap1;

	XMFLOAT4 m_offsets[14];

	FSSAOTexture m_ssaoBuffer[ESSAOTextureType::Max];
};