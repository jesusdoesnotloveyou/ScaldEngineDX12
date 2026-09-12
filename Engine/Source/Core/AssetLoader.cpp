#include "AssetLoader.h"

#include "Log/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace Scald;

DEFINE_LOG_CATEGORY_STATIC(LogAssetLoader);

NativeTexture AssimpLoader::LoadMaterial(const Path& relativePath, const std::string& fileName, bool sRGB)
{
    return NativeTexture();
}

//NativeMesh AssimpLoader::LoadMesh(const std::string& name)
//{
//    return NativeMesh();
//}

void AssimpLoader::LoadModel(const Path& relativePath)
{   
    Path absolutePath = std::filesystem::absolute(relativePath);
    if (absolutePath.empty())
    {
        Log::Get().LogMsg(LogAssetLoader, LogVerbosity::Error, std::format("Failed to get an absolute path for model: {}", relativePath.string()));
        return;
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(absolutePath.string(),
        aiProcess_Triangulate |
        aiProcess_GenNormals | // creates normal vectors for each vertex if the model doesn't contain normal vectors
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs |
        aiProcess_FindInvalidData |
        aiProcess_GenBoundingBoxes |
        // aiProcess_SplitLargeMeshes | splits large meshes into smaller sub-meshes which is useful if your rendering has a maximum number of vertices allowed and can only process smaller meshes
        // aiProcess_OptimizeMeshes | does the reverse by trying to join several meshes into one larger mesh, reducing drawing calls for optimization
        aiProcess_ConvertToLeftHanded);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
    {
        Log::Get().LogMsg(LogAssetLoader, LogVerbosity::Error, std::format("Failed to load model: {}. Error: {}", absolutePath.string(), importer.GetErrorString()));
        return;
    }

    ProcessNode(scene->mRootNode, scene);
}

void AssimpLoader::ProcessNode(const aiNode* node, const aiScene* scene)
{
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(node->mChildren[i], scene);
    }
}

NativeMesh AssimpLoader::ProcessMesh(const aiMesh* mesh, const aiScene* scene)
{   
    return NativeMesh();
}

void AssimpLoader::ProcessMaterial(const aiMaterial* material, aiTextureType type)
{

}