#include "Actor.h"
#include "d3dx12.h"
#include <DirectXMath.h>
#include "Engine/Engine.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Physics/CollisionManager.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/Behavior.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Math/Math.h"

using namespace DirectX;


// Constructor
Actor::Actor(Vector3 position, Vector3 rotation, Vector3 scale, bool isActive,  ACTOR_TAG tag) : 
	m_isActive(isActive), m_tag(tag)
{
	//Add TransformComponent by default
	m_pTransform = AddComponent<Transform>(position, rotation, scale);
}

// Destructor
Actor::~Actor()
{
	for (auto& component : m_components){
		component->OnDestroy();
	}
	for (auto& component : m_pendingComponents){
		component->OnDestroy();
	}
}

// Post-update (for late update)
void Actor::PreUpdate(float deltaTime)
{
	// Add pending components to the main component list
	for (auto& pendingComponent : m_pendingComponents) {
		m_components.push_back(std::move(pendingComponent));
	}
	m_pendingComponents.clear();

	// Post-update all components
	for (const auto& component : m_components) {
		// if component is destoryed, skip to next
		if (component->IsDestroyed()) continue;

		// If the component has not been started, call OnStart and mark it as started
		if(!component->IsStarted()) {
			component->OnStart();
			component->MarkAsStarted();
		}

		// Call PostUpdate for each component
		component->PreUpdate(deltaTime);
	}
}

// Update
void Actor::Update(float deltaTime)
{
	// Update all components
	for (const auto& component : m_components) {
		component->Update(deltaTime);
	}

	// Update all child actors
	for(auto& child : m_children) {
		child->Update(deltaTime);
	}
}

// Late update
void Actor::LateUpdate(float deltaTime)
{
	// Late update all components
	std::vector<Component*> destroyedComponents;
	for (const auto& component : m_components) {
		component->LateUpdate(deltaTime);
		if (component->IsDestroyed()) 
		{
			destroyedComponents.push_back(component.get());
		}
	}

	// Remove components marked for destruction
	for (auto& destroyed : destroyedComponents) {
		RemoveDestroyedComponents(destroyed);
	}
}

// Mark as actor as destroyed
void Actor::Destroy()
{
	m_destroyed = true;
}

// Check if actor is destroyed
bool Actor::IsDestroyed()
{
	return m_destroyed;
}

// Update world transform of this actor and all child actors;
void Actor::FlushTransform()
{
	if (m_pTransform) {
		m_pTransform->UpdateGeometry();
	}
}

// Remove components marked for destruction
void Actor::RemoveDestroyedComponents(Component* component)
{
	component->OnDestroy();
	m_components.erase(std::remove_if(m_components.begin(), m_components.end(),
		[component](const std::unique_ptr<Component>& c) { return c.get() == component; }),
		m_components.end());
}

// Add a child actor to the scene
void Actor::AddChildActorToScene(std::unique_ptr<Actor> child)
{
	if (m_pOwner) {
		m_pOwner->AddActorToPending(std::move(child));
	}
}
