#pragma once
#include "ObjectBase.h"
#include "AssimpNodeTransformAnim.h"

// Player class
class Player : public ObjectBase
{
public:	// Public functions
	Player(	// Constructor
		MESH_TYPE meshType,									// Mesh type
		DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f },	// Position
		DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f },	// Rotation
		DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f },		// Scale
		DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f },	// Velocity
		bool isActive = true,								// Active flag
		OBJECT_TAG tag = OBJECT_TAG::PLAYER,				// Object tag
		CollisionData::COLLISION_LAYER layer =
			CollisionData::COLLISION_LAYER::PLAYER,			// Collision layer
		DirectX::XMFLOAT3 colliderSetScale =
			{ 1.0f, 1.0f, 1.0f },							// Collider set scale
		DirectX::XMFLOAT3 colliderSetOffsetPosition =
			{ 0.0f, 0.0f, 0.0f },							// Collider set offset position
		DirectX::XMFLOAT3 colliderSetOffsetRotation =
			{ 0.0f, 0.0f, 0.0f }							// Collider set offset rotation
	);
	~Player();	// Destructor

	void CreateRenderModel(	// Empty implementation of pure virtual function
		TextureManager& textureManager,
		MeshManager& meshManager
	) override;

private:
	static constexpr double ANIM_TIME = 1.0 / 120.0; // Animation time
	double m_animTime = ANIM_TIME; // Animation time

private:
	// Overridden functions
	void UpdateOverride() override;				// Scene-specific update
	void ResolveCollisionsOverride() override;	// Scene-specific collision resolution
};