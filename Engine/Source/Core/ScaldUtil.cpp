#include "ScaldUtil.h"
// For texture loading
#include <DirectXTex.h>

void ScaldUtil::TransitionResource(ID3D12GraphicsCommandList* pCommandList, ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(pResource, stateBefore, stateAfter);
    pCommandList->ResourceBarrier(1u, &transition);
}

ComPtr<ID3D12Resource> ScaldUtil::CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, UINT64 byteSize, ComPtr<ID3D12Resource>& uploadBuffer)
{
    ComPtr<ID3D12Resource> defaultBuffer;

    auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    // Create the actual default buffer resource
    ThrowIfFailed(device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    // In order to copy CPU memory data into out default buffer, we need to create an intermediate upload heap
    ThrowIfFailed(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    // Describe the data we want to copy into the default buffer.
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;                    // For buffers the size of the data we are copying in bytes.
    subResourceData.SlicePitch = subResourceData.RowPitch;  // For buffers the size of the data we are copying in bytes.

    // Schedule to copy the data to the default buffer resource. At a high level, the helper function UpdateSubresources will copy the CPU memory
    // into the intermediate upload heap. Then, using ID3D12CommandList::CopySubresourceRegion, the intermediate upload heap data will be copied to mBuffer.
    TransitionResource(cmdList, defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0u, 1u, &subResourceData);
    TransitionResource(cmdList, defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

    // Note: uploadBuffer has to be kept alive after the above function calls because the command list has not been executed yet that performs the actual copy.
    // The caller can Release the uploadBuffer after it knows the copy has been executed.
    return defaultBuffer;
}

D3D12_GPU_VIRTUAL_ADDRESS ScaldUtil::GetGPUVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS address, UINT byteStride, UINT index /*= 0u*/)
{
    return address + (UINT64)(byteStride * index);
}

UINT ScaldUtil::CalcConstantBufferByteSize(const UINT byteSize)
{
    return (byteSize + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
}

ComPtr<ID3DBlob> ScaldUtil::CompileShader(const std::wstring& fileName, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target)
{
#if defined(_DEBUG) | defined(DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;
#else
    UINT compileFlags = D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;
#endif

    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors;

    hr = D3DCompileFromFile(fileName.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint.c_str(), target.c_str(), compileFlags, 0u, &byteCode, &errors);

    if (errors != nullptr) OutputDebugStringA((char*)errors->GetBufferPointer());

    ThrowIfFailed(hr);

    return byteCode;
}

void Texture::CreateTexture(const wchar_t* fileName, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, bool sRGB)
{
    HRESULT hr = S_OK; 
    
    std::filesystem::path filePath(fileName);
    if (!std::filesystem::exists(filePath))
    {   
        // file not found
        hr = 0x80070002;
        ThrowIfFailed(hr); // or assert maybe
    }

    // TODO: Runtime uploading
    //std::lock_guard<std::mutex> lock(ms_TextureCacheMutex);
    //auto iter = ms_TextureCache.find(fileName);
    
    // Now we only have compile time texture uploading
    // Assume that we upload every texture only once!

    // If the texture is loaded successfully, 
    // the TexMetadata structure contains the width, height, and (depth for 3D textures) as well as the DXGI_FORMAT of the loaded texture.
    TexMetadata metadata;
    // The ScratchImage class contains the pixel data for the texture.
    ScratchImage scratchImage;

    if (filePath.extension() == ".dds" || filePath.extension() == ".DDS")
    {
        ThrowIfFailed(LoadFromDDSFile(fileName, DDS_FLAGS_FORCE_RGB, &metadata, scratchImage));
    }
    else if (filePath.extension() == ".hdr" || filePath.extension() == ".HDR")
    {
        ThrowIfFailed(LoadFromHDRFile(fileName, &metadata, scratchImage));
    }
    else if (filePath.extension() == ".tga" || filePath.extension() == ".TGA")
    {
        ThrowIfFailed(LoadFromTGAFile(fileName, &metadata, scratchImage));
    }
    else
    {
        ThrowIfFailed(LoadFromWICFile(fileName, WIC_FLAGS_FORCE_RGB, &metadata, scratchImage));
    }

    // Force the texture format to be sRGB to convert to linear when sampling the texture in a shader.
    if (sRGB)
    {
        metadata.format = MakeSRGB(metadata.format);
    }

    D3D12_RESOURCE_DESC textureDesc = {};
    switch (metadata.dimension)
    {
        case TEX_DIMENSION_TEXTURE1D:
            textureDesc = CD3DX12_RESOURCE_DESC::Tex1D(metadata.format, static_cast<UINT64>(metadata.width), static_cast<UINT16>(metadata.arraySize));
            break;
        case TEX_DIMENSION_TEXTURE2D:
            textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(metadata.format, static_cast<UINT64>(metadata.width), static_cast<UINT>(metadata.height), static_cast<UINT16>(metadata.arraySize), static_cast<UINT16>(metadata.mipLevels));
            break;
        case TEX_DIMENSION_TEXTURE3D:
            textureDesc = CD3DX12_RESOURCE_DESC::Tex3D(metadata.format, static_cast<UINT64>(metadata.width), static_cast<UINT>(metadata.height), static_cast<UINT16>(metadata.depth));
            break;
        default:
            hr = E_INVALIDARG;
            ThrowIfFailed(hr);
            break;
    }

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(Resource.GetAddressOf())));

    // set filepath as a name placeholder because name is a const char* and i don't wanna fuck with that right now
    Resource->SetName(Filename.c_str());

    std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());

    const Image* pImages = scratchImage.GetImages();

    for (size_t i = 0; i < scratchImage.GetImageCount(); ++i)
    {
        auto& subresource = subresources[i];
        subresource.RowPitch = pImages[i].rowPitch;
        subresource.SlicePitch = pImages[i].slicePitch;
        subresource.pData = pImages[i].pixels;
    }

    const UINT firstSubresource = 0u;
    const UINT numSubresources = static_cast<UINT>(subresources.size());
    const UINT64 requiredSize = GetRequiredIntermediateSize(Resource.Get(), firstSubresource, numSubresources);

    auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);

    // Create an intermediate resource for uploading the subresources
    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE, 
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, 
        IID_PPV_ARGS(UploadHeap.GetAddressOf()))
    );

    UpdateSubresources(cmdList, Resource.Get(), UploadHeap.Get(), static_cast<UINT64>(0u), firstSubresource, numSubresources, subresources.data());   

    // If texture does not have mipmaps, they could be created in pixel shader
    if (subresources.size() < Resource->GetDesc().MipLevels)
    {
        //GenerateMips(texture);
    }
}