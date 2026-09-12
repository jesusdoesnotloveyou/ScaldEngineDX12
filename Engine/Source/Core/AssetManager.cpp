#include "AssetManager.h"
#include "AssetLoader.h"

#include "Log/Log.h"

#include <string>
#include <cstdint>
#include <unordered_map>
#include <memory>

using MaterialMap = std::unordered_map<std::string, uint32_t>;
using MeshMap = std::unordered_map<std::string, uint32_t>;
using TextureMap = std::unordered_map<std::string, uint32_t>;

using namespace Scald;
using namespace Microsoft::WRL;

DEFINE_LOG_CATEGORY_STATIC(LogAssetManager);

struct AssetManager::Impl
{
    ID3D12Device2* m_device = nullptr;

    std::unique_ptr<IAssetLoader> m_assetLoader;

    MaterialMap m_materials;
    MeshMap m_meshes;
    TextureMap m_textures;


    // Model contains meshes, materials
    // Model m_scene;
    // Assume for now that we have a single model for the entire scene, but we can extend this later to support multiple models.

    ComPtr<ID3D12Resource> m_uploadHeap;

    Impl(AssetImportLibrary library)
    {
        switch (library)
        {
            case AssetImportLibrary::Assimp:
                m_assetLoader = std::make_unique<AssimpLoader>();
                break;
        }
    }

    void LoadModel(const Path& relativePath)
    {
        if (!m_assetLoader)
        {
            Log::Get().LogMsg(LogAssetManager, LogVerbosity::Error, "Asset loader is not initialized.");
            return;
        }

        m_assetLoader->LoadModel(relativePath);
    }
};

AssetManager::AssetManager(AssetImportLibrary library)
    : m_pImpl(std::make_unique<Impl>(library))
{

}

AssetManager::~AssetManager() = default; // For PIMPL
