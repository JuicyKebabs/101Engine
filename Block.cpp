#include "Block.h"

Block::Block(
	MESH_TYPE meshType,
	DirectX::XMFLOAT3 position,
	DirectX::XMFLOAT3 rotation,
	DirectX::XMFLOAT3 scale,
	DirectX::XMFLOAT3 velocity,
	bool isActive,
	OBJECT_TAG tag,
	COLLISION_LAYER layer,
	DirectX::XMFLOAT3 colliderSetScale,
	DirectX::XMFLOAT3 colliderSetOffsetPosition,
	DirectX::XMFLOAT3 colliderSetOffsetRotation
) : Actor(
	meshType, position, rotation, scale, velocity, isActive, tag, layer,
	colliderSetScale, colliderSetOffsetPosition, colliderSetOffsetRotation
)
{
}

Block::~Block()
{
}

void Block::CreateRenderModel(
	TextureManager& pTextureManager,
	MeshManager& pMeshManager
)
{
	PSOKey key = PSO_KEY_OPAQUE.WithLighting();

	WorldRenderModel info;
	CreateRenderInfo(
		pTextureManager,					// Texture manager reference
		pMeshManager,						// Mesh manager reference
		&info,								// Pointer to the render info structure array
		GetMeshType(),						// Mesh type
		key,								// Pipeline state object key
		L"",								// Model data or texture file path
		true,								// Whether lighting is enabled
		false,								// Whether this is for post-effect
		BILLBOARD_TYPE::BILLBOARD_NONE,		// Billboard type
		false,								// Whether to flip U (only valid for model data)
		false								// Whether to flip V (only valid for model data)
	);

	SetRenderModel(info);
}

