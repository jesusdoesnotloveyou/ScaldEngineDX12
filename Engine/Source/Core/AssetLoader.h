#pragma once

#include <string>
#include <filesystem>

using Path = std::filesystem::path;

struct aiMaterial;
struct aiMesh;
struct aiNode;
struct aiScene;
enum aiTextureType;

namespace Scald
{
    struct NativeTexture
    {

    };

    struct NativeMesh
    {

    };

    struct NativeModel
    {

    };

    class IAssetLoader
    {
    public:
        IAssetLoader() = default;
        virtual ~IAssetLoader() noexcept = default;

        virtual NativeTexture LoadMaterial(const Path& relativePath, const std::string& fileName, bool sRGB = false) = 0;
        //virtual NativeMesh LoadMesh(const std::string& name) = 0;
        virtual void LoadModel(const Path& relativePath) = 0;
    };

    class AssimpLoader final : public IAssetLoader
    {
    public:
        AssimpLoader() = default;
        ~AssimpLoader() noexcept override = default;

        NativeTexture LoadMaterial(const Path& relativePath, const std::string& fileName, bool sRGB = false) override;
        //NativeMesh LoadMesh(const std::string& name) override;
        void LoadModel(const Path& relativePath) override;
        
    private:
        void ProcessNode(const aiNode* node, const aiScene* scene);
        NativeMesh ProcessMesh(const aiMesh* mesh, const aiScene* scene);
        void ProcessMaterial(const aiMaterial* material, aiTextureType type);
    };

}  // namespace Scald