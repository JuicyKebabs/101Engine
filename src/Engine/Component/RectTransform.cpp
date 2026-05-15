#include "RectTransform.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Window/WindowInfo.h"

void RectTransform::UpdateGeometry()
{
	if (!m_isDirty) return;

	// Get transform component of the parent actor
	auto owner = GetOwner();
	auto parent = owner ? owner->GetParent() : nullptr;
	auto parentRT = parent ? parent->GetComponentByClass<RectTransform>() : nullptr;

	// Calculate parent size (if parent has RectTransform, use its size; otherwise, use window size)
	Vector2 parentSize = parentRT ? parentRT->GetSize() 
		: Vector2(static_cast<float>(WindowInfo::GetInstance().GetWidth()), static_cast<float>(WindowInfo::GetInstance().GetHeight()));

	Vector2 anchorOffset = CalcAnchorOffset(m_anchorMode, parentSize);

	// Use anchor position as the local position
	Vector3 uiPosition(
		m_anchoredPosition.x + anchorOffset.x,
		m_anchoredPosition.y + anchorOffset.y,
		m_localTransform.position.z
	);

	// Use size as the local scale
	Vector3 uiScale(
		m_size.x,
		m_size.y,
		1.0f
	);

	// Calculate the world matrix
	Matrix4x4 localMatrix = Matrix4x4::CreateTRS(uiPosition, m_localTransform.rotation, uiScale);
	Matrix4x4 worldMatrix = parentRT ? localMatrix * parentRT->GetWorldMatrix() : localMatrix;
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

Vector2 RectTransform::CalcAnchorOffset(AnchorMode mode, const Vector2& parentSize) const
{
	const float halfWidth = parentSize.x * 0.5f;
	const float halfHeight = parentSize.y * 0.5f;

	switch (mode)
	{
	case AnchorMode::TopLeft:		return Vector2(-halfWidth, halfHeight);
	case AnchorMode::TopCenter:		return Vector2(0, halfHeight);
	case AnchorMode::TopRight:		return Vector2(halfWidth, halfHeight);
	case AnchorMode::MiddleLeft:	return Vector2(-halfWidth, 0.0f);
	case AnchorMode::MiddleCenter:	return Vector2(0.0f, 0.0f);
	case AnchorMode::MiddleRight:	return Vector2(halfWidth, 0.0f);
	case AnchorMode::BottomLeft:	return Vector2(-halfWidth, -halfHeight);
	case AnchorMode::BottomCenter:	return Vector2(0.0f, -halfHeight);
	case AnchorMode::BottomRight:	return Vector2(halfWidth, -halfHeight);
	default:						return Vector2(0.0f, 0.0f);	
	}
}
