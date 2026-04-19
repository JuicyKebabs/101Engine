#pragma once
#include <vector>
#include "Engine/Graphics/RenderData.h"
#include "Engine/Resource/Texture.h"

class TextureManager;
class MeshManager;

// Mesh description structure
struct MeshDesc
{
	MeshGPU* gpuHandle = nullptr;	// Mesh data
	UINT startIndex = 0;			// Start index
	UINT baseVertex = 0;			// Base vertex
	Vector3 boundsCenter{ 0,0,0 };	// Bounding sphere center for sorting
	float boundsRadius = 0.0f;		// Bounding sphere radius for sorting
};

// Material description structure
struct MaterialDesc
{
	TextureHandle textureHandle = InvalidTextureHandle;	// Texture handle for the material
	PSOKey psoKey{};									// Pipeline State Object key
	Vector4 baseColor{ 1,1,1,1 };						// Base color for rendering (can be used to tint the mesh)
	bool lightingEnabled = true;						// Lighting enabled flag
};

// Render template structure for a single mesh
// This structure contains all the necessary static information to render
struct SubmeshRenderTemplate
{
	MeshDesc meshDesc;			// Mesh data and rendering settings
	MaterialDesc materialDesc;	// Material information and rendering settings
};

using MeshRenderTemplate = std::vector<SubmeshRenderTemplate>;	// Alias for a render template consisting of multiple submesh templates

// Render template structure for a sprite
struct SpriteRenderTemplate
{
	MaterialDesc materialDesc;							// Material description for the sprite
	BillboardType billboardType = BillboardType::None;	// Billboard type for the sprite
};

// Material input structure for creating material descriptions
struct MaterialInput
{
	std::wstring texturePath;						// Path to the texture to use for this material
	Vector4 baseColor{ 1,1,1,1 };					// Base color for rendering (can be used to tint the mesh)
	PSOKey psoKey = PSO_KEY_DEFAULT::MESH_OPAQUE;	// Pipeline State Object key for this material
	bool lightingEnabled = true;					// Lighting enabled flag
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
	static MeshRenderTemplate CreateMeshRenderTemplate(
		MeshManager& meshManager,
		TextureManager& textureManager,
		const MeshInput& meshInput,
		const MaterialInput& materialInput
	);

	static MeshRenderTemplate CreateMeshRenderTemplateFromDefaultMesh(
		MeshManager& meshManager,
		TextureManager& textureManager,
		DEFAULT_MESH desc,
		const MaterialInput& materialInput
	);

	static SpriteRenderTemplate CreateSpriteRenderTemplate(
		TextureManager& textureManager,
		const MaterialInput& materialInput,
		BillboardType billboardType
	);

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

	static MeshRenderTemplate BuildRenderTemplate(
		MeshManager& meshManager,
		Model& model,
		MaterialDesc& materialDesc
	);
};