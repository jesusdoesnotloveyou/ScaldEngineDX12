#pragma once

#include "DXHelper.h"
#include "Mesh.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace Scald
{
class aiMaterial;
class aiMesh;
class aiNode;
class aiScene;

//class SceneNode;
//class Visitor;

enum EBuiltInMeshes
{
   BOX = 0,
   SPHERE,
   GEOSPHERE,
   GRID,
   NUM_BUILTIN_MESHES
};

using MeshLookup_t = std::unordered_map<MeshID, MeshData<>>;
// using ModelLookup_t = std::unordered_map<ModelID, Model>;

static MeshID LAST_USED_MESH_ID = EBuiltInMeshes::NUM_BUILTIN_MESHES;

class Scene final
{
   friend class Engine;

private:
   MeshLookup_t m_meshes;
   // ModelLookup_t m_models;
   std::array<MeshData<>, EBuiltInMeshes::NUM_BUILTIN_MESHES> m_buildInMeshes;

public:
   Scene();
   ~Scene() noexcept = default;

   MeshData<> GetBuiltInMesh(EBuiltInMeshes meshType);

private:
   MeshID AddMesh(const MeshData<>& mesh);
   MeshID AddMesh(MeshData<>&& mesh);

   void CreateBuildInMeshes();
};
}  // namespace Scald

//class Scene
//{
//public:
//    Scene() = default;
//    ~Scene() = default;
//
//    void SetRootNode(std::shared_ptr<SceneNode> node) { m_RootNode = node; }
//
//    std::shared_ptr<SceneNode> GetRootNode() const { return m_RootNode; }
//
//    /**
//     * Get the AABB of the scene.
//     * This returns the AABB of the root node of the scene.
//     */
//    DirectX::BoundingBox GetAABB() const;
//
//    /**
//     * Accept a visitor.
//     * This will first visit the scene, then it will visit the root node of the scene.
//     */
//    virtual void Accept(Visitor& visitor);
//
//protected:
//    friend class CommandList;
//
//    /**
//     * Load a scene from a file on disc.
//     */
//    bool LoadSceneFromFile(CommandList& commandList, const std::wstring& fileName, const std::function<bool(float)>& loadingProgress);
//
//    /**
//     * Load a scene from a string.
//     * The scene can be preloaded into a byte array and the
//     * scene can be loaded from the loaded byte array.
//     *
//     * @param scene The byte encoded scene file.
//     * @param format The format of the scene file.
//     */
//    bool LoadSceneFromString(CommandList& commandList, const std::string& sceneStr, const std::string& format);
//
//private:
//    void ImportScene(CommandList& commandList, const aiScene& scene, std::filesystem::path parentPath);
//    void ImportMaterial(CommandList& commandList, const aiMaterial& material, std::filesystem::path parentPath);
//    void ImportMesh(CommandList& commandList, const aiMesh& mesh);
//    std::shared_ptr<SceneNode> ImportSceneNode(CommandList& commandList, std::shared_ptr<SceneNode> parent, const aiNode* aiNode);
//
//    using MaterialMap = std::map<std::string, std::shared_ptr<Material>>;
//    using MaterialList = std::vector<std::shared_ptr<Material>>;
//    using MeshList = std::vector<std::shared_ptr<Mesh>>;
//
//    MaterialMap m_MaterialMap;
//    MaterialList m_Materials;
//    MeshList m_Meshes;
//
//    std::shared_ptr<SceneNode> m_RootNode;
//
//    std::wstring m_SceneFile;
//};