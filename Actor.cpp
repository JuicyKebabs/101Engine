#include "Actor.h"
#include "d3dx12.h"
#include <DirectXMath.h>
#include "Engine.h"
#include "Renderer.h"
#include "RenderData.h"
#include "CollisionManager.h"
#include "TransformComponent.h"
#include "BehaviorComponent.h"

using namespace DirectX;


// Constructor
Actor::Actor(XMFLOAT3 position, XMFLOAT3 rotation, XMFLOAT3 scale, bool isActive,  OBJECT_TAG tag) : 
	m_isActive(isActive), m_tag(tag)
{
	// Create components container
	m_pComponentsContainer = std::make_unique<ComponentsContainer>();

	//Add TransformComponent by default
	m_pComponentsContainer->AddComponent<TransformComponent>(this, position, rotation, scale);
}

// Destructor
Actor::~Actor()
{
}

// Update
void Actor::Update(float deltaTime)
{
	auto behavior = GetComponent<BehaviorComponent>();
	if (behavior) {
		behavior->Update(deltaTime);
	}
	m_passedFrames++;
}

// Late update
void Actor::LateUpdate()
{
}

//アクティブかどうかを取得
const bool Actor::IsActive() const
{
	return m_isActive;
}

// Set active flag
void Actor::SetActive(bool isActive)
{
	m_isActive = isActive;
}

// Set object tag
void Actor::SetTag(OBJECT_TAG tag)
{
	m_tag = tag;
}

// Get object tag
OBJECT_TAG Actor::GetTag() const
{
	return m_tag;
}