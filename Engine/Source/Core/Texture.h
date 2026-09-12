#pragma once

#include "DXHelper.h"
#include "FileSystemObject.h"

namespace Scald
{
    using Microsoft::WRL::ComPtr;

    class Texture final : public FileSystemObject
    {
    public:
        enum class TextureType : UINT
        {
            NONE = 0,
            SKYCUBE,
            ALBEDO,
            NORMAL,
            ROUGHNESS,
            METALNESS,
            AO,
            MAX = 7
        };
    public:
        Texture() {}
        Texture(const char* name, const wchar_t* fileName, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureType type = TextureType::ALBEDO, bool sRGB = false)
            : Name(std::string(name))
            , Filename(std::wstring(fileName))
            , Type(type)
        {
            // ThrowIfFailed(CreateDDSTextureFromFile12(device, cmdList, Filename.c_str(), Resource, UploadHeap));
            CreateTexture(fileName, device, cmdList, sRGB);
        }
        
        virtual ~Texture() override = default;

        // Begin of FileSystemObject interface
        virtual void Copy() override;
        virtual void Move() override;
        virtual void Delete() override;
        // End of FileSystemObject interface

        TextureType Type = TextureType::NONE;
        // Unique material name for lookup.
        std::string Name;
        std::wstring Filename;

        ComPtr<ID3D12Resource> Resource = nullptr;

    private:
        void CreateTexture(const wchar_t* fileName, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, bool sRGB);

    private:
        // UploadHeap name is from Luna's book. It is an intermediate upload heap used to copy CPU memory data into the default buffer resource on the GPU.
        ComPtr<ID3D12Resource> UploadHeap = nullptr;
    };
}  // namespace Scald