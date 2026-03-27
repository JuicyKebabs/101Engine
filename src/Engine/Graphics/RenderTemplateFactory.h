#pragma once
#include <vector>
#include "Engine/Graphics/RenderData.h"

class TextureManager;
class MeshManager;

// Mesh description structure
struct MeshDesc
{
	MeshGPU* gpuHandle = nullptr;	// Mesh data
	UINT startIndex = 0;			// Start index
	UINT baseVertex = 0;			// Base vertex
};

// Material description structure
struct MaterialDesc
{
	uint32_t srvIndex = UINT32_MAX;	// SRV index (for textures)
	PSOKey psoKey{};				// Pipeline State Object key
	Vector4 baseColor{ 1,1,1,1 };	// Base color for rendering (can be used to tint the mesh)
	bool lightingEnabled = true;	// Lighting enabled flag
};

// Render template structure for a single mesh
// This structure contains all the necessary static information to render
struct SubmeshRenderTemplate
{
	MeshDesc meshDesc;			// Mesh data and rendering settings
	MaterialDesc materialDesc;	// Material information and rendering settings
};

using RenderTemplate = std::vector<SubmeshRenderTemplate>;	// Alias for a render template consisting of multiple submesh templates

// Material input structure for creating material descriptions
struct MaterialInput
{
	std::wstring texturePath;		// Path to the texture to use for this material
	Vector4 baseColor{ 1,1,1,1 };	// Base color for rendering (can be used to tint the mesh)
	PSOKey psoKey{};				// Pipeline State Object key for this material
	bool lightingEnabled = true;	// Lighting enabled flag
};

// Mesh input structure for creating render templates
struct MeshInput
{
	std::wstring modelPath;	// Path to the model file
	bool inverseU = false;	// Whether to invert the U coordinate of the texture (useful for certain model formats)
	bool inverseV = false;	// Whether to invert the V coordinate of the texture (useful for certain model formats)
};

// Factory class for creating render templates based on mesh and material descriptions
class RenderTemplateFactory
{
public:
	static RenderTemplate CreateRenderTemplate(
		MeshManager& meshManager,
		TextureManager& textureManager,
		const MeshInput& meshInput,
		const MaterialInput& materialInput
	);

	static RenderTemplate CreateRenderTemplateFromDefaultMesh(
		MeshManager& meshManager,
		TextureManager& textureManager,
		DEFAULT_MESH desc,
		const MaterialInput& materialInput
	);

private:
	static MaterialDesc BuildMaterialDesc(
		TextureManager& textureManager,
		const MaterialInput& input
	);

	static Model LoadModelFromFile(
		const std::wstring& path,
		bool inverseU, 
		bool inverseV
	);

	static Model LoadDefaultModel(DEFAULT_MESH type);

	static RenderTemplate BuildRenderTemplate(
		MeshManager& meshManager,
		Model& model,
		MaterialDesc& materialDesc
	);
};