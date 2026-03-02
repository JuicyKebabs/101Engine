#include "TransformComponent.h"

using namespace DirectX;

// Get world matrix
const XMMATRIX TransformComponent::GetWorldMatrix() const
{
	XMMATRIX T = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(m_rotation.x),
		XMConvertToRadians(m_rotation.y),
		XMConvertToRadians(m_rotation.z));
	XMMATRIX S = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
	return S * R * T;
}

// Get position
const XMFLOAT3 TransformComponent::GetPosition() const
{
	return m_position;
}

// Get rotation
const XMFLOAT3 TransformComponent::GetRotation() const
{
	return m_rotation;
}

// Get scale
const XMFLOAT3 TransformComponent::GetScale() const
{
	return m_scale;
}

// Set position
void TransformComponent::SetPosition(XMFLOAT3 position)
{
	m_position = position;
}

// Set rotation
void TransformComponent::SetRotation(XMFLOAT3 rotation)
{
	m_rotation = rotation;
}

// Set scale
void TransformComponent::SetScale(XMFLOAT3 scale)
{
	m_scale = scale;
}