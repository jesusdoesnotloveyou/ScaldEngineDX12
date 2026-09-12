#pragma once

#include <memory>
#include <cstdint>

namespace Scald
{
    class AssetManager final
    {
    public:
        enum class AssetImportLibrary : uint32_t
        {
            Assimp = 0u,
            tynyobjloader,
            gltf,
            MAX
        };

        AssetManager(AssetImportLibrary library);
        ~AssetManager() noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_pImpl;
    };
}  // namespace Scald