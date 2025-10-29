#pragma once

#include "DXHelper.h"

using Microsoft::WRL::ComPtr;

class ScaldUtil
{
public:
    static ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, UINT64 byteSize, ComPtr<ID3D12Resource>& uploadBuffer);

    static UINT CalcConstantBufferByteSize(const UINT byteSize);

	static ComPtr<ID3DBlob> CompileShader(const std::wstring& fileName, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target);
};

// Defines a subrange of geometry in a MeshGeometry.  This is for when multiple
// geometries are stored in one vertex and index buffer.  It provides the offsets
// and data needed to draw a subset of geometry stores in the vertex and index 
// buffers so that we can implement the technique described by Figure 6.3.
struct SubmeshGeometry
{
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	INT BaseVertexLocation = 0;

	// Bounding box of the geometry defined by this submesh. 
	BoundingBox Bounds;
};

struct MeshGeometry
{
	MeshGeometry(const char* name)
		:
		Name(std::string(name))
	{
	}

	// Give it a name so we can look it up by name.
	std::string Name;

	// System memory copies.  Use Blobs because the vertex/index format can be generic.
	// It is up to the client to cast appropriately.  
	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	// the actual default buffer resource
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	// an intermediate upload heap in order to copy CPU memory data into out default buffer
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	// Data about the buffers.
	UINT VertexByteStride = 0u;
	UINT VertexBufferByteSize = 0u;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferByteSize = 0u;

	// A MeshGeometry may store multiple geometries in one vertex/index buffer.
	// Use this container to define the Submesh geometries so we can draw
	// the Submeshes individually.
	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = VertexByteStride;
		vbv.SizeInBytes = VertexBufferByteSize;

		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;

		return ibv;
	}

	// We can free this memory after we finish upload to the GPU.
	void DisposeUploaders()
	{
		VertexBufferUploader = nullptr;
		IndexBufferUploader = nullptr;
	}

	template<typename TVertex, typename  TIndex = uint16_t>
	void CreateGPUBuffers(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<TVertex>& vertices, const std::vector<TIndex>& indices = std::vector<TIndex>(0))
	{
		static_assert(std::is_same<TIndex, unsigned>() || std::is_same<TIndex, unsigned short>());

		const UINT64 vbByteSize = vertices.size() * sizeof(TVertex);
		const UINT ibByteSize = (UINT)indices.size() * sizeof(TIndex);

		if (vbByteSize)
		{
			// Create system buffer for copy vertices data
			ThrowIfFailed(D3DCreateBlob(vbByteSize, &VertexBufferCPU));
			CopyMemory(VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
			// Create GPU resource
			VertexBufferGPU = ScaldUtil::CreateDefaultBuffer(device, cmdList, vertices.data(), vbByteSize, VertexBufferUploader);
			// Initialize the vertex buffer view.
			VertexBufferByteSize = (UINT)vbByteSize;
			VertexByteStride = sizeof(TVertex);
		}

		if (ibByteSize)
		{
			// Create system buffer for copy indices data
			ThrowIfFailed(D3DCreateBlob(ibByteSize, &IndexBufferCPU));
			CopyMemory(IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
			// Create GPU resource
			IndexBufferGPU = ScaldUtil::CreateDefaultBuffer(device, cmdList, indices.data(), ibByteSize, IndexBufferUploader);
			// Initialize the index buffer view.
			IndexBufferByteSize = ibByteSize;
			IndexFormat = (std::is_same<TIndex, unsigned short>()) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		}
	}
};

struct Texture
{
	Texture() {}

	Texture(const char* name, const wchar_t* fileName, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
		: Name(std::string(name))
		, Filename(std::wstring(fileName))
	{
		ThrowIfFailed(CreateDDSTextureFromFile12(device, cmdList, Filename.c_str(), Resource, UploadHeap));
	}
	// Unique material name for lookup.
	std::string Name;

	std::wstring Filename;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};