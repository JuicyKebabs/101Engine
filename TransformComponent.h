#pragma once
#include <DirectXMath.h>
#include "Component.h"

// TransformComponent Class
class TransformComponent : public Component
{
public:
	TransformComponent(Actor* owner, DirectX::XMFLOAT3 position, DirectX::XMFLOAT3 rotation, DirectX::XMFLOAT3 scale) :
		Component(owner), m_position(position), m_rotation(rotation), m_scale(scale) {
	}
	virtual ~TransformComponent() = default;

	const DirectX::XMMATRIX GetWorldMatrix() const;	// Get world matrix
	const DirectX::XMFLOAT3 GetPosition() const;	// Get position
	const DirectX::XMFLOAT3 GetRotation() const;	// Get rotation
	const DirectX::XMFLOAT3 GetScale() const;		// Get scale

	void SetPosition(DirectX::XMFLOAT3 position);	// Set position
	void SetRotation(DirectX::XMFLOAT3 rotation);	// Set rotation
	void SetScale(DirectX::XMFLOAT3 scale);			// Set scale

private:
	DirectX::XMFLOAT3 m_position{ 0.0f, 0.0f, 0.0f };	// Position
	DirectX::XMFLOAT3 m_rotation{ 0.0f, 0.0f, 0.0f };	// Rotation
	DirectX::XMFLOAT3 m_scale{ 1.0f, 1.0f, 1.0f };		// Scale
};