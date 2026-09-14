#include "BuiltinMeshAssets.h"
#include "Engine/Graphics/RenderData.h"
#include <array>

namespace
{
	const std::array<BuiltinMeshAsset, 6> BuiltinMeshes{
		BuiltinMeshAsset{ Guid::FromString("{8C00A61A-5A71-4E70-9232-7C99B17E9831}"), "BuiltIn/Meshes/Quad", DefaultMesh::Quad },
		BuiltinMeshAsset{ Guid::FromString("{B2C1C7D2-A905-4951-92EE-9D85CE3FA93C}"), "BuiltIn/Meshes/Cube", DefaultMesh::Cube },
		BuiltinMeshAsset{ Guid::FromString("{C5DA21EA-A784-4B3B-A2F7-91D2644A933A}"), "BuiltIn/Meshes/Circle", DefaultMesh::Circle },
		BuiltinMeshAsset{ Guid::FromString("{9F63052B-8F13-4A69-9B6D-F886F995984E}"), "BuiltIn/Meshes/Sphere", DefaultMesh::Sphere },
		BuiltinMeshAsset{ Guid::FromString("{FC8A4038-099A-40F6-855B-D11874C1E923}"), "BuiltIn/Meshes/Capsule", DefaultMesh::Capsule },
		BuiltinMeshAsset{ Guid::FromString("{419762E8-378D-483A-A5E9-147063863C2B}"), "BuiltIn/Meshes/Cylinder", DefaultMesh::Cylinder },
	};
}

std::span<const BuiltinMeshAsset> GetBuiltinMeshAssets()
{
	return BuiltinMeshes;
}

const BuiltinMeshAsset* FindBuiltinMeshAsset(const Guid& guid)
{
	for (const BuiltinMeshAsset& asset : BuiltinMeshes)
	{
		if (asset.guid == guid) return &asset;
	}
	return nullptr;
}
