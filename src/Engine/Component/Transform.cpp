#include "Transform.h"
#include "Engine/Actor/Actor.h"

using namespace DirectX;

Transform::Transform(Vector3 localPosition, Vector3 localEulerDeg, Vector3 localScale, const std::string& name)
	: Component(name)
{
	m_localTransform = { localPosition, Quaternion::CreateFromEulerDeg(localEulerDeg), localScale };
}

// Called when the component is initialized
void Transform::OnStart()
{
	UpdateGeometry();
}

// Called every frame before Update to perform pre-update tasks
void Transform::PreUpdate(float deltaTime)
{
}

// Called every frame after Update to perform late updates
void Transform::LateUpdate(float deltaTime)
{
}

// Called when the component is destroyed
void Transform::OnDestroy()
{
}

// Mark the local transform as dirty (has been modified since last world transform update)
void Transform::MarkDirty()
{
	if (m_isDirty) return;

	m_isDirty = true;

	auto owner = GetOwner();
	auto children = owner ? owner->GetChildren() : std::vector<Actor*>{};

	for(auto& child : children)
	{
		auto transform = child->GetComponentByClass<Transform>();
		if (transform) transform->MarkDirty();
	}
}

// Rotate local transform by a quaternion
void Transform::RotateLocalByQuat(Quaternion quaternion)
{
	m_localTransform.rotation = quaternion * m_localTransform.rotation;
	MarkDirty();
}

// Rotate local transform by Euler angles in degrees
void Transform::RotateLocalByEulerDeg(Vector3 eulerDeg)
{
	Quaternion rotationQuat = Quaternion::CreateFromEulerDeg(eulerDeg);
	m_localTransform.rotation = m_localTransform.rotation * rotationQuat;
	MarkDirty();
}

// Rotate local transform by Euler angles in radians
void Transform::RotateLocalByEulerRad(Vector3 eulerRad)
{
	Quaternion rotationQuat = Quaternion::CreateFromEulerRad(eulerRad);
	m_localTransform.rotation = m_localTransform.rotation * rotationQuat;
	MarkDirty();
}

// Rotate local transform around X axis by an angle in degrees
void Transform::RotateLocalByXDeg(float angleDeg)
{
	Quaternion rotationQuat = Quaternion::CreateFromEulerDeg(Vector3(angleDeg, 0.0f, 0.0f));
	m_localTransform.rotation = m_localTransform.rotation * rotationQuat;
	MarkDirty();
}

// Rotate local transform around Y axis by an angle in degrees
void Transform::RotateLocalByYDeg(float angleDeg)
{
	Quaternion rotationQuat = Quaternion::CreateFromEulerDeg(Vector3(0.0f, angleDeg, 0.0f));
	m_localTransform.rotation = m_localTransform.rotation * rotationQuat;
	MarkDirty();
}

// Rotate local transform around Z axis by an angle in degrees
void Transform::RotateLocalByZDeg(float angleDeg)
{
	Quaternion rotationQuat = Quaternion::CreateFromEulerDeg(Vector3(0.0f, 0.0f, angleDeg));
	m_localTransform.rotation = m_localTransform.rotation * rotationQuat;
	MarkDirty();
}

// Set local position
void Transform::SetLocalPosition(Vector3 position)
{
	m_localTransform.position = position;
	MarkDirty();
}

// Set local scale
void Transform::SetLocalScale(Vector3 scale)
{
	m_localTransform.scale = scale;
	MarkDirty();
}

// Set local rotation using quaternion
void Transform::SetLocalRotationQuat(Quaternion quaternion)
{
	m_localTransform.rotation = quaternion;
	MarkDirty();
}

// Set local rotation using Euler angles in degrees
void Transform::SetLocalRotationEulerDeg(Vector3 eulerDeg)
{
	m_localTransform.rotation = Quaternion::CreateFromEulerDeg(eulerDeg);
	MarkDirty();
}

// Set local rotation using Euler angles in radians
void Transform::SetLocalRotationEulerRad(Vector3 eulerRad)
{
	m_localTransform.rotation = Quaternion::CreateFromEulerRad(eulerRad);
	MarkDirty();
}

// Set local transform
void Transform::SetLocalTransform(const Transform3D& localTransform)
{
	m_localTransform = localTransform;
	MarkDirty();
}

// Get local position
Vector3 Transform::GetLocalPosition() const
{
	return m_localTransform.position;
}

// Get local scale
Vector3 Transform::GetLocalScale() const
{
	return m_localTransform.scale;
}

// Get local rotation as quaternion
Quaternion Transform::GetLocalRotationQuat() const
{
	return m_localTransform.rotation;
}

// Get local rotation as Euler angles in degrees
Vector3 Transform::GetLocalRotationEulerDeg() const
{
	return m_localTransform.rotation.ToEulerDeg();
}

// Get local rotation as Euler angles in radians
Vector3 Transform::GetLocalRotationEulerRad() const
{
	return m_localTransform.rotation.ToEulerRad();
}

// Get local transform
const Transform3D& Transform::GetLocalTransform() const
{
	return m_localTransform;
}

// Get local matrix
Matrix4x4 Transform::GetLocalMatrix() const
{
	return m_localMatrix;
}

// Get world position
Vector3 Transform::GetWorldPosition() const
{
	return m_worldTransform.position;
}

// Get world rotation as quaternion
Quaternion Transform::GetWorldRotationQuat() const
{
	return m_worldTransform.rotation;
}

// Get world rotation as Euler angles in degrees
Vector3 Transform::GetWorldRotationEulerDeg() const
{
	return m_worldTransform.rotation.ToEulerDeg();
}

// Get world rotation as Euler angles in radians
Vector3 Transform::GetWorldRotationEulerRad() const
{
	return m_worldTransform.rotation.ToEulerRad();
}

// Get world scale
Vector3 Transform::GetWorldScale() const
{
	return m_worldTransform.scale;
}

// Get world transform
const Transform3D& Transform::GetWorldTransform() const 
{
	return m_worldTransform;
}

// Get world matrix
Matrix4x4 Transform::GetWorldMatrix() const
{
	return m_worldMatrix;
}

// Get forward direction
Vector3 Transform::GetLocalForward() const
{
	return m_localMatrix.TransformDirection(Vector3::Forward());
}

// Get backward direction
Vector3 Transform::GetLocalBack() const
{
	return m_localMatrix.TransformDirection(Vector3::Backward());
}

// Get right direction
Vector3 Transform::GetLocalRight() const
{
	return m_localMatrix.TransformDirection(Vector3::Right());
}

// Get left direction
Vector3 Transform::GetLocalLeft() const
{
	return m_localMatrix.TransformDirection(Vector3::Left());
}

// Get up direction
Vector3 Transform::GetLocalUp() const
{
	return m_localMatrix.TransformDirection(Vector3::Up());
}

// Get down direction
Vector3 Transform::GetLocalDown() const
{
	return m_localMatrix.TransformDirection(Vector3::Down());
}

// Get forward direction
Vector3 Transform::GetWorldForward() const
{
	return m_worldMatrix.TransformDirection(Vector3::Forward());
}

// Get backward direction
Vector3 Transform::GetWorldBack() const
{
	return m_worldMatrix.TransformDirection(Vector3::Backward());
}

// Get right direction
Vector3 Transform::GetWorldRight() const
{
	return m_worldMatrix.TransformDirection(Vector3::Right());
}

// Get left direction
Vector3 Transform::GetWorldLeft() const
{
	return m_worldMatrix.TransformDirection(Vector3::Left());
}

// Get up direction
Vector3 Transform::GetWorldUp() const
{
	return m_worldMatrix.TransformDirection(Vector3::Up());
}

// Get down direction
Vector3 Transform::GetWorldDown() const
{
	return m_worldMatrix.TransformDirection(Vector3::Down());
}

// Transform a point from local space to world space
Vector3 Transform::TransformPoint(Vector3 localPoint) const
{
	return m_worldMatrix.TransformPoint(localPoint);
}

// Transform a direction from local space to world space (ignoring position)
Vector3 Transform::TransformDirection(Vector3 localDirection) const
{
	return m_worldMatrix.TransformDirection(localDirection);
}

// Transform a point from world space to local space
Vector3 Transform::InverseTransformPoint(Vector3 localPoint) const
{
	return m_worldMatrix.Inverse().TransformPoint(localPoint);
}

// Transform a direction from world space to local space (ignoring position)
Vector3 Transform::InverseTransformDirection(Vector3 localDirection) const
{
	return m_worldMatrix.Inverse().TransformDirection(localDirection);
}

// Update world transform based on local transform and parent's world transform
void Transform::UpdateGeometry()
{
	if(!m_isDirty) return;

	auto owner = GetOwner();
	auto parent = owner ? owner->GetParent() : nullptr;
	auto parentTransform = parent ? parent->GetComponentByClass<Transform>() : nullptr;

	if (parentTransform)
	{
		m_worldTransform = CombineTransform3D(parentTransform->GetWorldTransform(), m_localTransform);
	}
	else
	{
		m_worldTransform = m_localTransform;
	}

	m_localMatrix = m_localTransform.GetMatrix();
	m_worldMatrix = m_worldTransform.GetMatrix();

	m_worldGeneration++;

	m_isDirty = false;
}

// Get world generation
uint64_t Transform::GetWorldGeneration() const
{
	return m_worldGeneration;
}
