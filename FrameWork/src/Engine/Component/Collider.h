#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include "Engine/Component/Component.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Physics/CollisionData.h"

// Forward declaration
class Actor;

// Enumration for collider types
enum class ColliderType
{
	BOX,		// Box
	SPHERE,		// Sphere
	CAPSULE,	// Capsule
	None		// None
};

// Structure for box collider
struct BoxCollider
{
	Vector3 center;		// Center point
	Vector3 scale;		// Size
	Vector3 defaultScale;	// Initial size
};

// Structure for sphere collider
struct SphereCollider
{
	Vector3 center;	// Center point
	float radius;				// Radius
	float defaultRadius;		// Initial radius
};

// Structure for capsule collider
struct CapsuleCollider
{
	Vector3 pointA;	// Endpoint A (bottom center)
	Vector3 pointB;	// Endpoint B (top center)
	float cylHeight;			// Height between endpoints
	float defaultHeight;		// Initial height
	float radius;				// Radius
	float defaultRadius;		// Initial radius
};

// Structure for axis-aligned bounding box (AABB)
struct AABB
{
	Vector3 min;	// Minimum point
	Vector3 max;	// Maximum point
};

class Collider : public Component
{
public:
	struct ParamDesc
	{
		Vector3 localCenter = Vector3(0, 0, 0);				// Local center position
		Quaternion localRotation = Quaternion::Identity();	// Local rotation
		Vector3 localScale = Vector3(1, 1, 1);				// Local scale
		ColliderType type = ColliderType::None;				// Collider type
		CollisionLayer layer = CollisionLayer::Default;		// Collision layer
		bool isTrigger = false;								// Trigger flag (whether to ignore physical collisions)
		std::string name = "Collider";						// Component name (optional, can be used for debugging or identification)
	};

public:
	Collider() = default;
	~Collider() = default;
	void SetParams(const ParamDesc& desc) {
		m_localTransform = { desc.localCenter, desc.localRotation, desc.localScale };
		m_type = desc.type;
		m_layer = desc.layer;
		m_isTrigger = desc.isTrigger;
		m_layerMask = MakeLayerMask(m_layer);
		SetName(desc.name);
		m_isDirty = true;
	}

	void OnStartOverride() override;
	void PreUpdateOverride(float deltaTime) override;
	void UpdateOverride(float deltaTime) override;
	void LateUpdateOverride(float deltaTime) override;
	void OnDestroyOverride() override;
	void Flush();

	// Manage collision information
	void AddCollisionInfo(const CollisionInfo& info) { m_collisionInfos.push_back(info); };
	void ClearCollisionInfos() { m_collisionInfos.clear(); }

	// Runtime update functions
	void UpdateCollider(Vector3 ownerScale);
	void UpdateAABB();
	void SetPreviousState();

	// Getters
	const std::vector<CollisionInfo>& GetCollisionInfos() const { return m_collisionInfos; }
	ColliderType GetType() const { return m_type; }
	CollisionLayer GetLayer() const { return m_layer; }
	LayerMask GetLayerMask() const { return m_layerMask; }
	bool IsTrigger() const { return m_isTrigger; }
	const AABB& GetSewptAABB() const { return m_sweptAABB; }
	const BoxCollider& GetCurrentBoxCollider() const { return m_currentBoxCollider; }
	const BoxCollider& GetPreviousBoxCollider() const { return m_previousBoxCollider; }
	const SphereCollider& GetCurrentSphereCollider() const { return m_currentSphereCollider; }
	const SphereCollider& GetPreviousSphereCollider() const { return m_previousSphereCollider; }
	const CapsuleCollider& GetCurrentCapsuleCollider() const { return m_currentCapsuleCollider; }
	const CapsuleCollider& GetPreviousCapsuleCollider() const { return m_previousCapsuleCollider; }
	Matrix4x4 GetWorldMatrix() const { return Matrix4x4::CreateTRS(m_worldTransformCurrent.position, m_worldTransformCurrent.rotation, m_worldTransformCurrent.scale); }
	TagId GetOwnerTag() const { return m_ownerTag; }
	std::vector<CollisionInfo>& GetCollisionInfos() { return m_collisionInfos; }
	bool isDetected() const { return m_isDetected; }
	bool deleteFlag() const { return m_deleteFlag; }
	bool isActive() const { return m_isActive; }
	const Transform3D& GetWorldTransformCurrent() const { return m_worldTransformCurrent; }
	const Transform3D& GetWorldTransformPrevious() const { return m_worldTransformPrevious; }

	// Setters
	void SetDetected(bool flag) { m_isDetected = flag; }
	void SetDeleteFlag(bool flag) { m_deleteFlag = flag; }
	void SetActive(bool flag) { m_isActive = flag; m_isDirty = true; }

	// Serialization and deserialization methods
	bool Serialize(nlohmann::json& outJson) const override;
	bool Deserialize(const nlohmann::json& json) override;

private:
	std::vector<CollisionInfo> m_collisionInfos;

	ColliderType m_type = ColliderType::None;			// Collider type
	CollisionLayer m_layer = CollisionLayer::Default;	// Collision layer
	LayerMask m_layerMask = 0;							// Collision layer mask
	bool m_isTrigger = false;							// Trigger flag (whether to ignore physical collisions)

	// Axis-aligned bounding box
	AABB m_currentAABB;		// Current axis-aligned bounding box (for BroadPhase)
	AABB m_previousAABB;	// Previous axis-aligned bounding box (for BroadPhase)
	AABB m_sweptAABB;		// SweptAABB (for BroadPhase)

	BoxCollider m_currentBoxCollider;			// Current box collider
	BoxCollider m_previousBoxCollider;			// Previous box collider
	SphereCollider m_currentSphereCollider;		// Current sphere collider
	SphereCollider m_previousSphereCollider;	// Previous sphere collider
	CapsuleCollider m_currentCapsuleCollider;	// Current capsule collider
	CapsuleCollider m_previousCapsuleCollider;	// Previous capsule collider

	Transform3D m_localTransform{};				// Local transform (position, rotation, scale) relative to the owner object
	Transform3D m_worldTransformCurrent{};		// Current world transform
	Transform3D m_worldTransformPrevious{};		// Previous world transform

	Vector3 m_scaleOffset;	// Size difference with the object

	TagId m_ownerTag = TAG_NONE;	// Owner object's tag

	bool m_isActive = true;		// Active flag
	bool m_isDetected = false;	// Collision detection flag (for rendering)
	bool m_deleteFlag = false;	// Delete flag

	bool m_isDirty = true;				// Flag to indicate if the collider needs to be updated
	uint64_t m_transformGeneration = 0;	// Generation count of the owner's transform to check for changes

private:
	void ChackIfTransformChanged();	// Check if the owner's transform has changed
	void RefreshWorldTransform();	// Refresh the world transform based on the owner's transform

	// Collider update functions
	void UpdateBoxCollider(Vector3 ownerScale);		// Update box collider
	void UpdateSphereCollider(Vector3 ownerScale);	// Update sphere collider
	void UpdateCapsuleCollider(Vector3 ownerScale);	// Update capsule collider

	// AABB update functions
	void UpdateAABBBox();			// Update AABB for box collider
	void UpdateAABBSphere();		// Update AABB for sphere collider
	void UpdateAABBCapsule();		// Update AABB for capsule collider
	void MakeSweptAABB();			// Create SweptAABB
};