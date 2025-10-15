#pragma once

#include "Common/DXHelper.h"
#include "Core/Shapes.h"

namespace Scald
{
	enum EBuiltInMeshes
	{
		BOX = 0,
		SPHERE,
		GRID,
		NUM_BUILTIN_MESHES
	};

	using MeshLookup_t = std::unordered_map<MeshID, MeshData<>>;
	//using ModelLookup_t = std::unordered_map<ModelID, Model>;
	
	static MeshID LAST_USED_MESH_ID = EBuiltInMeshes::NUM_BUILTIN_MESHES;

	class Scene final
	{
		friend class Engine;
	
	private:
		MeshLookup_t m_meshes;
		//ModelLookup_t m_models;	
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
}
