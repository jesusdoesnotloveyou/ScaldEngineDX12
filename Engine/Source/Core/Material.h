#pragma once

#include "DXHelper.h"
#include "FileSystemObject.h"

namespace Scald
{
    using namespace DirectX;

    class Material final : public FileSystemObject
    {
    public:
        Material(const char* name)
            : Name(std::string(name))
        {
        }

        virtual ~Material() override = default;

        // Begin of FileSystemObject interface
        virtual void Copy() override;
        virtual void Move() override;
        virtual void Delete() override;
        // End of FileSystemObject interface

        Material(const char* name, int materialBufferIndex, int diffuseSrvHeapIndex, int normalSrvHeapIndex = -1)
            : Name(std::string(name))
            , MatBufferIndex(materialBufferIndex)
            , DiffuseSrvHeapIndex(diffuseSrvHeapIndex)
            , NormalSrvHeapIndex(normalSrvHeapIndex)
        {
        }

    public:
        std::string Name;

        XMFLOAT4 DiffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
        XMFLOAT3 FresnelR0 = {0.01f, 0.01f, 0.01f};
        float Roughness = 0.25f;
        // could be used for material animation (water for instance)
        // Index into constant/structured buffer corresponding to this material (to map with render item).
        int MatBufferIndex = -1;
        // Index into SRV heap for diffuse texture. Index of corresponding texture in Texture2D[n]
        int DiffuseSrvHeapIndex = -1;
        // Index into SRV heap for normal texture.
        int NormalSrvHeapIndex = -1;
        // Index into SRV heap for roughness texture.
        int RoughnessSrvHeapIndex = -1;
        // Index into SRV heap for metalness texture.
        int MetalnessSrvHeapIndex = -1;

        int NumFramesDirty = RenderCommon::kNumFrameResources;
        XMMATRIX MatTransform = XMMatrixIdentity();
    };
}  // namespace Scald