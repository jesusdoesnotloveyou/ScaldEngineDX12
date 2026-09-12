#include "Texture.h"
#include <DirectXTex.h>

using namespace Scald;

void Texture::Copy() {}

void Texture::Move() {}

void Texture::Delete() {}

void Texture::CreateTexture(const wchar_t* fileName, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, bool sRGB)
{
    HRESULT hr = S_OK;

    std::filesystem::path filePath(fileName);
    if (!std::filesystem::exists(filePath))
    {
        // file not found
        hr = 0x80070002;
        ThrowIfFailed(hr);  // or assert maybe
    }

    // TODO: Runtime uploading
    // std::lock_guard<std::mutex> lock(ms_TextureCacheMutex);
    // auto iter = ms_TextureCache.find(fileName);

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
        case TEX_DIMENSION_TEXTURE1D: textureDesc = CD3DX12_RESOURCE_DESC::Tex1D(metadata.format, static_cast<UINT64>(metadata.width), static_cast<UINT16>(metadata.arraySize)); break;
        case TEX_DIMENSION_TEXTURE2D:
            textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
                metadata.format, static_cast<UINT64>(metadata.width), static_cast<UINT>(metadata.height), static_cast<UINT16>(metadata.arraySize), static_cast<UINT16>(metadata.mipLevels));
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
    ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(Resource.GetAddressOf())));

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
    ThrowIfFailed(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(UploadHeap.GetAddressOf())));

    UpdateSubresources(cmdList, Resource.Get(), UploadHeap.Get(), static_cast<UINT64>(0u), firstSubresource, numSubresources, subresources.data());

    // If texture does not have mipmaps, they could be created in pixel shader
    if (subresources.size() < Resource->GetDesc().MipLevels)
    {
        // GenerateMips(texture);
    }
}