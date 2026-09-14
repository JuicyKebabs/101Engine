#pragma once
#include "Engine/Core/GUID/Guid.h"
#include <span>
#include <string_view>

enum class DefaultMesh;

// Catalog identity for a mesh supplied by the engine rather than by a project file.
// This table is the single source of truth for its persistent GUID, picker path,
// and procedural geometry type.
struct BuiltinMeshAsset
{
	Guid guid;
	std::string_view path;
	DefaultMesh mesh;
};

std::span<const BuiltinMeshAsset> GetBuiltinMeshAssets();
const BuiltinMeshAsset* FindBuiltinMeshAsset(const Guid& guid);
