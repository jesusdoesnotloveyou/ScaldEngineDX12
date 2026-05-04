#include "stdafx.h"

#include "SSAO.h"
#include "FrameResource.h"
#include "Common/ScaldMath.h"

#include <DirectXPackedVector.h>

using namespace DirectX::PackedVector;

SSAO::SSAO(ID3D12Device* device, ID3D12GraphicsCommandList* pCommandList, UINT width, UINT height)
	: m_device(device)
{
	OnResize(width, height);

    BuildOffsetVectors();
    BuildRandomVectorTexture(pCommandList);
}

void SSAO::OnResize(UINT newWidth, UINT newHeight)
{
	if (m_renderTargetWidth != newWidth || m_renderTargetHeight != newHeight)
	{
		m_renderTargetWidth = newWidth;
		m_renderTargetHeight = newHeight;

		m_viewport.TopLeftX = 0.0f;
		m_viewport.TopLeftY = 0.0f;
		m_viewport.Width = m_renderTargetWidth / 2.0f;
		m_viewport.Height = m_renderTargetHeight / 2.0f;
		m_viewport.MinDepth = 0.0f;
		m_viewport.MaxDepth = 1.0f;

		m_scissorRect = { 0L, 0L, static_cast<LONG>(m_renderTargetWidth / 2), static_cast<LONG>(m_renderTargetHeight / 2) };

		BuildResources();
	}
}

void SSAO::BuildResources()
{
    // Free the old resources if they exist.
    m_ambientMap0 = nullptr;
    m_ambientMap1 = nullptr;

    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    // Ambient occlusion maps are at half resolution.
    texDesc.Width = m_renderTargetWidth / 2;
    texDesc.Height = m_renderTargetHeight / 2;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = SSAO::AmbientMapFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    CD3DX12_CLEAR_VALUE optClear = CD3DX12_CLEAR_VALUE(AmbientMapFormat, ambientClearColor);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &optClear,
        IID_PPV_ARGS(&m_ambientMap0)));

    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &optClear,
        IID_PPV_ARGS(&m_ambientMap1)));

    SCALD_NAME_D3D12_OBJECT(m_ambientMap0, L"AmbientMap0");
    SCALD_NAME_D3D12_OBJECT(m_ambientMap1, L"AmbientMap1");
}

ID3D12Resource* SSAO::GetAmbientMap()
{
    return m_ambientMap0.Get();
}

void SSAO::GetOffsetVectors(XMFLOAT4 offsets[14])
{
    std::copy(&m_offsets[0], &m_offsets[14], &offsets[0]);
}

std::vector<float> SSAO::CalcGaussWeights(float sigma)
{
    float twoSigma2 = 2.0f * sigma * sigma;

    // Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
    // For example, for sigma = 3, the width of the bell curve is 
    int blurRadius = (int)ceil(2.0f * sigma);

    assert(blurRadius <= MaxBlurRadius);

    std::vector<float> weights;
    weights.resize(2 * blurRadius + 1);

    float weightSum = 0.0f;

    for (int i = -blurRadius; i <= blurRadius; ++i)
    {
        float x = (float)i;

        weights[i + blurRadius] = expf(-x * x / twoSigma2);

        weightSum += weights[i + blurRadius];
    }

    // Divide by the sum so all the weights add up to 1.0.
    for (int i = 0; i < weights.size(); ++i)
    {
        weights[i] /= weightSum;
    }

    return weights;
}

void SSAO::BuildDescriptors(
    ID3D12Resource* depthStencilBuffer,
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
    const UINT cbvSrvUavDescriptorSize,
    const UINT rtvDescriptorSize)
{
    // Save references to the descriptors. The Ssao reserves heap space for 3 contiguous Srvs and 2 contiguous Rtvs

    m_ssaoBuffer[ESSAOTextureType::AmbientMap0].m_hCpuSrv = hCpuSrv;
    m_ssaoBuffer[ESSAOTextureType::AmbientMap1].m_hCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    m_ssaoBuffer[ESSAOTextureType::RandomVectors].m_hCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);

    m_ssaoBuffer[ESSAOTextureType::AmbientMap0].m_hGpuSrv = hGpuSrv;
    m_ssaoBuffer[ESSAOTextureType::AmbientMap1].m_hGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    m_ssaoBuffer[ESSAOTextureType::RandomVectors].m_hGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);

    m_ssaoBuffer[ESSAOTextureType::AmbientMap0].m_hCpuRtv = hCpuRtv;
    m_ssaoBuffer[ESSAOTextureType::AmbientMap1].m_hCpuRtv = hCpuRtv.Offset(1, rtvDescriptorSize);

    //  Create the descriptors
    RebuildDescriptors(depthStencilBuffer);
}

void SSAO::RebuildDescriptors(ID3D12Resource* depthStencilBuffer)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    m_device->CreateShaderResourceView(m_randomVectorMap.Get(), &srvDesc, m_ssaoBuffer[ESSAOTextureType::RandomVectors].m_hCpuSrv);

    srvDesc.Format = AmbientMapFormat;
    m_device->CreateShaderResourceView(m_ambientMap0.Get(), &srvDesc, m_ssaoBuffer[ESSAOTextureType::AmbientMap0].m_hCpuSrv);
    m_device->CreateShaderResourceView(m_ambientMap1.Get(), &srvDesc, m_ssaoBuffer[ESSAOTextureType::AmbientMap1].m_hCpuSrv);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = AmbientMapFormat;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;
    m_device->CreateRenderTargetView(m_ambientMap0.Get(), &rtvDesc, m_ssaoBuffer[ESSAOTextureType::AmbientMap0].m_hCpuRtv);
    m_device->CreateRenderTargetView(m_ambientMap1.Get(), &rtvDesc, m_ssaoBuffer[ESSAOTextureType::AmbientMap1].m_hCpuRtv);
}

void SSAO::SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso)
{
    m_ssaoPso = ssaoPso;
    m_ssaoBlurPso = ssaoBlurPso;
}

// TO DO: move calcs from stack
void SSAO::BuildRandomVectorTexture(ID3D12GraphicsCommandList* pCommandList)
{
    D3D12_RESOURCE_DESC texDesc = {};
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = (UINT64)0u;
    texDesc.Width = (UINT64)256u;
    texDesc.Height = 256u;
    texDesc.DepthOrArraySize = (UINT16)1u;
    texDesc.MipLevels = (UINT16)1u;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc = { 1u, 0u };
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_randomVectorMap)));

    SCALD_NAME_D3D12_OBJECT(m_randomVectorMap, L"RandomVectorMap");

    // In order to copy CPU memory data into our default buffer, we need to create an intermediate upload heap. 

    const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_randomVectorMap.Get(), 0u, num2DSubresources);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_randomVectorMapUploadBuffer.GetAddressOf())));

    XMCOLOR initData[256 * 256];
    for (int i = 0; i < 256; ++i)
    {
        for (int j = 0; j < 256; ++j)
        {
            // Random vector in [0,1].  We will decompress in shader to [-1,1].
            XMFLOAT3 v(ScaldMath::RandF(), ScaldMath::RandF(), ScaldMath::RandF());

            initData[i * 256 + j] = XMCOLOR(v.x, v.y, v.z, 0.0f);
        }
    }

    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = 256 * sizeof(XMCOLOR);
    subResourceData.SlicePitch = subResourceData.RowPitch * 256;

    // Schedule to copy the data to the default resource, and change states.
    // Note that mCurrSol is put in the GENERIC_READ state so it can be read by a shader.

    pCommandList->ResourceBarrier(1u, &CD3DX12_RESOURCE_BARRIER::Transition(m_randomVectorMap.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources(pCommandList, m_randomVectorMap.Get(), m_randomVectorMapUploadBuffer.Get(), (UINT64)0u, 0u, num2DSubresources, &subResourceData);
    pCommandList->ResourceBarrier(1u, &CD3DX12_RESOURCE_BARRIER::Transition(m_randomVectorMap.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void SSAO::BuildOffsetVectors()
{
    // Start with 14 uniformly distributed vectors.  We choose the 8 corners of the cube
    // and the 6 center points along each cube face.  We always alternate the points on 
    // opposites sides of the cubes.  This way we still get the vectors spread out even
    // if we choose to use less than 14 samples.

    // 8 cube corners
    m_offsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
    m_offsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

    m_offsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
    m_offsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

    m_offsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
    m_offsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

    m_offsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
    m_offsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

    // 6 centers of cube faces
    m_offsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
    m_offsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

    m_offsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
    m_offsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

    m_offsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
    m_offsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

    for (int i = 0; i < 14; ++i)
    {
        // Create random lengths in [0.25, 1.0].
        float s = ScaldMath::RandF(0.25f, 1.0f);

        XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&m_offsets[i]));

        XMStoreFloat4(&m_offsets[i], v);
    }
}

void SSAO::Compute(ID3D12GraphicsCommandList* pCommandList, FrameResource* currFrameResource, int blurPassesCount)
{
    auto ssaoCB = currFrameResource->SsaoCB->Get();

    pCommandList->RSSetViewports(1u, &m_viewport);
    pCommandList->RSSetScissorRects(1u, &m_scissorRect);

    ScaldUtil::ResourceBarrier(pCommandList, m_ambientMap0.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);

    pCommandList->OMSetRenderTargets(1u, &m_ssaoBuffer[ESSAOTextureType::AmbientMap0].m_hCpuRtv, TRUE, nullptr);
    pCommandList->ClearRenderTargetView(m_ssaoBuffer[ESSAOTextureType::AmbientMap0].m_hCpuRtv, ambientClearColor, 0u, nullptr);

    pCommandList->SetGraphicsRootConstantBufferView(0u, ssaoCB->GetGPUVirtualAddress());
    pCommandList->SetGraphicsRoot32BitConstant(1u, 0u, 0u); // no blur
    pCommandList->SetGraphicsRootDescriptorTable(4u, m_ssaoBuffer[ESSAOTextureType::RandomVectors].m_hGpuSrv);

    pCommandList->SetPipelineState(m_ssaoPso);

    // Draw fullscreen quad (check commented code for different approach to draw full quad)
    pCommandList->IASetVertexBuffers(0u, 0u, nullptr);
    pCommandList->IASetIndexBuffer(nullptr);

    /*pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandList->DrawInstanced(6u, 1u, 0u, 0u);*/
    
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    pCommandList->DrawInstanced(4u, 1u, 0u, 0u);

    ScaldUtil::ResourceBarrier(pCommandList, m_ambientMap0.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
    
    BlurAmbientMap(pCommandList, currFrameResource, blurPassesCount);
}

void SSAO::BlurAmbientMap(ID3D12GraphicsCommandList* pCommandList, FrameResource* currFrameResource, int blurCount)
{
    pCommandList->SetPipelineState(m_ssaoBlurPso);

    auto ssaoCB = currFrameResource->SsaoCB->Get();
    pCommandList->SetGraphicsRootConstantBufferView(0, ssaoCB->GetGPUVirtualAddress());

    for (int i = 0; i < blurCount; ++i)
    {
        BlurAmbientMap(pCommandList, true);
        BlurAmbientMap(pCommandList, false);
    }
}

void SSAO::BlurAmbientMap(ID3D12GraphicsCommandList* pCommandList, bool horzBlur)
{
    ID3D12Resource* output = nullptr;
    CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;

    // Ping-pong the two ambient map textures as we apply
    // horizontal and vertical blur passes.
    if (horzBlur == true)
    {
        output = m_ambientMap1.Get();
        inputSrv = m_ssaoBuffer[ESSAOTextureType::AmbientMap0].m_hGpuSrv;
        outputRtv = m_ssaoBuffer[ESSAOTextureType::AmbientMap1].m_hCpuRtv;
        pCommandList->SetGraphicsRoot32BitConstant(1u, 1u, 0u);
    }
    else
    {
        output = m_ambientMap0.Get();
        inputSrv = m_ssaoBuffer[ESSAOTextureType::AmbientMap1].m_hGpuSrv;
        outputRtv = m_ssaoBuffer[ESSAOTextureType::AmbientMap0].m_hCpuRtv;
        pCommandList->SetGraphicsRoot32BitConstant(1u, 0u, 0u);
    }

    ScaldUtil::ResourceBarrier(pCommandList, output, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);

    pCommandList->ClearRenderTargetView(outputRtv, ambientClearColor, 0, nullptr);

    pCommandList->OMSetRenderTargets(1u, &outputRtv, TRUE, nullptr);

    // Normal/depth map still bound from the next subpass

    // Bind the input ambient map to second texture table
    pCommandList->SetGraphicsRootDescriptorTable(4u, inputSrv);

    // Draw fullscreen quad
    pCommandList->IASetVertexBuffers(0u, 0u, nullptr);
    pCommandList->IASetIndexBuffer(nullptr);
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    pCommandList->DrawInstanced(4u, 1u, 0u, 0u);

    ScaldUtil::ResourceBarrier(pCommandList, output, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
}