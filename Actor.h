#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include "ColliderSet.h"
#include "SharedStruct.h"
#include "RenderData.h"
#include "AssimpNodeTransformAnim.h"

// Forward declaration
class Renderer;
class TextureManager;
class MeshManager;

// Actor Class
// This defines the all elements that exist in the game world
class Actor
{
public:
	Actor(	// Constructor
		MESH_TYPE meshType,								// Mesh type
		DirectX::XMFLOAT3 position,						// Position
		DirectX::XMFLOAT3 rotation,						// Rotation
		DirectX::XMFLOAT3 scale,						// Scale
		DirectX::XMFLOAT3 velocity,						// Velocity
		bool isActive = true,							// Active flag
		OBJECT_TAG tag = OBJECT_TAG::NONE,				// Object tag
		COLLISION_LAYER layer =
			COLLISION_LAYER::DEFAULT,	// Collision layer
		DirectX::XMFLOAT3 colliderSetScale =
			{1.0f, 1.0f, 1.0f},							// Collider set scale
		DirectX::XMFLOAT3 colliderSetOffsetPosition =
			{ 0.0f, 0.0f, 0.0f },						// Collider set offset position
		DirectX::XMFLOAT3 colliderSetOffsetRotation =
			{ 0.0f, 0.0f, 0.0f }						// Collider set offset rotation
	);
	~Actor();	// Destructor

	void Update();														// Update
	void SubmitDraws(Renderer& renderer);							// Render information submission
	void ResolveCollisions();											// Collision resolution
	void AddCollisionInfo(const CollisionInfo& info);	// Add collision information
	void ClearCollisionInfos();											// Clear collision information
	virtual void CreateRenderModel(										// Create render model
		TextureManager& textureManager,
		MeshManager& meshManager
	) = 0;

	// Getter
	const DirectX::XMMATRIX GetWorldMatrix() const;	// Get world matrix
	const DirectX::XMFLOAT3 GetPosition() const;	// Get position
	const DirectX::XMFLOAT3 GetRotation() const;	// Get rotation
	const DirectX::XMFLOAT3 GetScale() const;		// Get scale
	const DirectX::XMFLOAT4 GetColor() const;		// Get color RGBA
	const DirectX::XMFLOAT3 GetVelocity() const;	// Get velocity
	const bool IsActive() const;					// Get active status
	const bool IsDrawn() const;						// Get drawn flag
	ColliderSet* GetColliderSet() const;			// Get collider
	OBJECT_TAG GetTag() const;						// Get object tag
	const TexSplitInfo& GetTexSplitInfo() const;	// Get texture split info struct
	MESH_TYPE GetMeshType() const;					// Get mesh type
	WorldRenderModel GetRenderModel();				// Get render info array
	NodeAnimatorSet* GetNodeAnimatorSet();			// Get node animation set

	// Setter
	void SetPosition(DirectX::XMFLOAT3 position);						// Set position
	void SetRotation(DirectX::XMFLOAT3 rotation);						// Set rotation
	void SetScale(DirectX::XMFLOAT3 scale);								// Set scale
	void SetColor(DirectX::XMFLOAT4 color);								// Set color RGBA
	void SetVelocity(DirectX::XMFLOAT3 velocity);						// Set velocity
	void SetActive(bool isActive);										// Set active flag
	void SetDrawn(bool isDrawn);										// Set drawn flag
	void SetTexSplitInfo(TexSplitInfo info);							// Set texture split info struct
	void SetRenderModel(const WorldRenderModel& renderModel);			// Set render info array
	void SetNodeAnimatorSet(const NodeAnimatorSet& nodeAnimatorSet);	// Set node animator set

private:	// Non-public member variables
	DirectX::XMFLOAT3 m_position{ 0.0f, 0.0f, 0.0f };		// Position
	DirectX::XMFLOAT3 m_rotation{ 0.0f, 0.0f, 0.0f };		// Rotation
	DirectX::XMFLOAT3 m_scale{ 1.0f, 1.0f, 1.0f };			// Scale
	DirectX::XMFLOAT4 m_color{ 1.0f, 1.0f, 1.0f, 1.0f };	// Color RGBA
	DirectX::XMFLOAT3 m_velocity{ 0.0f, 0.0f, 0.0f };		// Velocity

	// Flags
	bool m_isActive = false;	// Active flag
	bool m_isDrawn = true;		// Draw flag

	// Components
	ColliderSet* m_pColliderSet = nullptr;	// Pointer to collider set
	WorldRenderModel m_renderModel{};	// Render info array

	OBJECT_TAG m_tag = OBJECT_TAG::NONE; // Object tag
	TexSplitInfo m_texSplitInfo{}; // Texture split info struct

	MESH_TYPE m_meshType = MESH_TYPE::IMPORT; // Mesh type

	NodeAnimatorSet m_nodeAnimatorSet{}; // Node animator set

	int m_passedFrames = 0; // Elapsed frame count

private:	// Non-public member variables
	virtual void UpdateOverride() = 0;				// Scene-specific update
	virtual void ResolveCollisionsOverride() = 0;	// Scene-specific collision resolution
	void UpdateAnimation();							// Animation update
};