#pragma once

#include "Common/DXHelper.h"

struct Mesh
{
public:
    Mesh() = default;
    Mesh(const Mesh& mesh) = default;
    Mesh(Mesh&& mesh) noexcept = default;
    ~Mesh() noexcept = default;

public:
    struct Material* material = nullptr;

    XMMATRIX TexTransform = XMMatrixIdentity();

    ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

    ComPtr<ID3D12Resource> VertexBufferUpload = nullptr;
    ComPtr<ID3D12Resource> IndexBufferUpload = nullptr;

    D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    uint32_t VertexByteStride = 0u;
    uint32_t VertexBufferByteSize = 0u;
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
    uint32_t IndexBufferByteSize = 0u;

    uint32_t IndexCount = 0u;
    uint32_t VertexCount = 0u;

public:
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
        vbv.SizeInBytes = VertexBufferByteSize;
        vbv.StrideInBytes = VertexByteStride;
        return vbv;
    }

    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
    {
        D3D12_INDEX_BUFFER_VIEW ibv = {};
        ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = IndexFormat;
        ibv.SizeInBytes = IndexBufferByteSize;
        return ibv;
    }
};