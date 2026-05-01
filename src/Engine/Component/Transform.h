#pragma once
#include <DirectXMath.h>
#include "Component.h"
#include "Engine/Core/Math/Math.h"

// TransformComponent Class
class Transform : public Component
{
public:
	struct InitDesc : public Component::InitDesc
	{
		Vector3 localPosition;
		Vector3 localEulerDeg;
		Vector3 localScale;
		InitDesc(std::string name = "Transform") : Component::InitDesc(name) {}
		InitDesc(Vector3 localPosition, Vector3 localEulerDeg, Vector3 localScale, std::string name = "Transform")
			: Component::InitDesc(name), localPosition(localPosition), localEulerDeg(localEulerDeg), localScale(localScale) {}
	};

public:
	Transform() = default;
	virtual ~Transform() = default;
	void Init(const InitDesc& desc) {
		m_localTransform.position = desc.localPosition;
		m_localTransform.rotation = Quaternion::CreateFromEulerDeg(desc.localEulerDeg);
		m_localTransform.scale = desc.localScale;
		Component::Init(desc);
	}

	// Overrides
	void OnStartOverride() override;
	void PreUpdateOverride(float deltaTime) override;
	void UpdateOverride(float deltaTime) override {};
	void LateUpdateOverride(float deltaTime) override;
	void OnDestroyOverride() override;

	// Dirty flag
	void MarkDirty();	// Mark the local transform as dirty (has been modified since last world transform update)

	// Rotation methods
	void RotateLocalByQuat(Quaternion quaternion);	// Rotate local transform by a quaternion
	void RotateLocalByEulerDeg(Vector3 eulerDeg);	// Rotate local transform by Euler angles in degrees
	void RotateLocalByEulerRad(Vector3 eulerRad);	// Rotate local transform by Euler angles in radians
	
	// Axis-specific rotation methods
	void RotateLocalByXDeg(float angleDeg);	// Rotate local transform around X axis by an angle in degrees
	void RotateLocalByYDeg(float angleDeg);	// Rotate local transform around Y axis by an angle in degrees
	void RotateLocalByZDeg(float angleDeg);	// Rotate local transform around Z axis by an angle in degrees

	// Setters (all for local transform)
	void SetLocalPosition(Vector3 position);			// Set local position
	void SetLocalScale(Vector3 scale);					// Set local scale
	void SetLocalRotationQuat(Quaternion quaternion);	// Set local rotation using quaternion
	void SetLocalRotationEulerDeg(Vector3 eulerDeg);	// Set local rotation using Euler angles in degrees
	void SetLocalRotationEulerRad(Vector3 eulerRad);	// Set local rotation using Euler angles in radians
	void SetLocalTransform(const Transform3D& localTransform);	// Set local transform

	// Local Getters 
	Vector3 GetLocalPosition() const;				// Get local position
	Quaternion GetLocalRotationQuat() const;		// Get local rotation as quaternion
	Vector3 GetLocalRotationEulerDeg() const;		// Get local rotation as Euler angles in degrees(It should not be used in game logic. This is for editor or debugging)
	Vector3 GetLocalRotationEulerRad() const;		// Get local rotation as Euler angles in radians(It should not be used in game logic. This is for editor or debugging)
	Vector3 GetLocalScale() const;					// Get local scale
	const Transform3D& GetLocalTransform() const;	// Get local transform
	Matrix4x4 GetLocalMatrix() const;				// Get local matrix

	// World Getters
	Vector3 GetWorldPosition() const;				// Get world position
	Quaternion GetWorldRotationQuat() const;		// Get world rotation as quaternion
	Vector3 GetWorldRotationEulerDeg() const;		// Get world rotation as Euler angles in degrees(It should not be used in game logic. This is for editor or debugging)
	Vector3 GetWorldRotationEulerRad() const;		// Get world rotation as Euler angles in radians(It should not be used in game logic. This is for editor or debugging)
	Vector3 GetWorldScale() const;					// Get world scale
	const Transform3D& GetWorldTransform() const;	// Get world transform
	Matrix4x4 GetWorldMatrix() const;				// Get world matrix

	// Basis
	Vector3 GetLocalForward() const;	// Get local forward direction
	Vector3 GetLocalBack() const;		// Get local back direction
	Vector3 GetLocalRight() const;		// Get local right direction
	Vector3 GetLocalLeft() const;		// Get local left direction
	Vector3 GetLocalUp() const;			// Get local up direction
	Vector3 GetLocalDown() const;		// Get local down direction
	Vector3 GetWorldForward() const;	// Get world forward direction
	Vector3 GetWorldBack() const;		// Get world back direction
	Vector3 GetWorldRight() const;		// Get world right direction
	Vector3 GetWorldLeft() const;		// Get world left direction
	Vector3 GetWorldUp() const;			// Get world up direction
	Vector3 GetWorldDown() const;		// Get world down direction

	// Conversions
	Vector3 TransformPoint(Vector3 localPoint) const;					// Transform a point from local space to world space
	Vector3 TransformDirection(Vector3 localDirection) const;			// Transform a direction from local space to world space (ignoring position)
	Vector3 InverseTransformPoint(Vector3 worldPoint) const;			// Transform a point from world space to local space
	Vector3 InverseTransformDirection(Vector3 worldDirection) const;	// Transform a direction from world space to local space (ignoring position)

	void UpdateGeometry();					// Update world transform based on local transform and parent's world transform
	uint64_t GetWorldGeneration() const;	// Get world transform generation counter (incremented every time world transform is updated)

private:
	Transform3D m_localTransform{};	// Local transform data (position, rotation, scale)
	Transform3D m_worldTransform{};	// World transform data (position, rotation, scale)
	Matrix4x4 m_localMatrix{};		// Local transformation matrix
	Matrix4x4 m_worldMatrix{};		// World transformation matrix

	bool m_isDirty = true;	// Flag to indicate if the local transform has been modified since last world transform update

	uint64_t m_worldGeneration = 0;	// World transform generation counter (incremented every time world transform is updated)
};