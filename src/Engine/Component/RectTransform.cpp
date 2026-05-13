#include "RectTransform.h"
#include "Engine/Actor/Actor.h"

void RectTransform::UpdateGeometry()
{
	if (!m_isDirty) return;

	// Get transform component of the parent actor
	auto owner = GetOwner();
	auto parent = owner ? owner->GetParent() : nullptr;
	auto parentTransform = parent ? parent->GetComponentByClass<Transform>() : nullptr;

	// Use anchor position as the local position
	Vector3 uiPosition(
		m_anchoredPosition.x,
		m_anchoredPosition.y,
		m_localTransform.position.z
	);

	// Use size delta as the local scale
	Vector3 uiScale(
		m_sizeDelta.x,
		m_sizeDelta.y,
		1.0f
	);

	// Calculate the world matrix
	Matrix4x4 localMatrix = Matrix4x4::CreateScale(uiScale) * Matrix4x4::CreateFromQuaternion(m_localTransform.rotation) * Matrix4x4::CreateTranslation(uiPosition);
	Matrix4x4 worldMatrix = parentTransform ? localMatrix * parentTransform->GetWorldMatrix() : localMatrix;
	m_worldMatrix = worldMatrix;

	// Save world transform by decomposing the world matrix
	m_worldMatrix.Decompose(
		m_worldTransform.position, 
		m_worldTransform.rotation, 
		m_worldTransform.scale
	);

	m_worldGeneration++;
	m_isDirty = false;
}