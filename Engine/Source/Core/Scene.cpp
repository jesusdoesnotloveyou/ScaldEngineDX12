#include "Scene.h"
#include "Shapes.h"

using namespace Scald;

Scene::Scene()
{
   CreateBuildInMeshes();
}

MeshData<> Scene::GetBuiltInMesh(EBuiltInMeshes meshType)
{
   return m_buildInMeshes[meshType];
}

MeshID Scene::AddMesh(const MeshData<>& mesh)
{
   // !!!!!!! need a mutex
   MeshID id = LAST_USED_MESH_ID++;
   m_meshes[id] = mesh;
   return id;
}

MeshID Scene::AddMesh(MeshData<>&& mesh)
{
   // !!!!!!! need a mutex
   MeshID id = LAST_USED_MESH_ID++;
   m_meshes[id] = mesh;
   return id;
}

void Scene::CreateBuildInMeshes()
{
   m_buildInMeshes[EBuiltInMeshes::BOX] = Shapes::CreateBox(1.0f, 1.0f, 1.0f);
   m_buildInMeshes[EBuiltInMeshes::SPHERE] = Shapes::CreateSphere(1.0f, 16u, 16u);
   m_buildInMeshes[EBuiltInMeshes::GEOSPHERE] = Shapes::CreateGeosphere(1.0f, 3u);
   m_buildInMeshes[EBuiltInMeshes::GRID] = Shapes::CreateGrid(100.0f, 100.0f, 2u, 2u);
}