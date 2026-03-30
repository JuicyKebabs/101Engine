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
	for (auto& component : m_componentPtrs){
		component->OnDestroy();
	}
	for (auto& pending : m_pendingComponents){
		pending.instance->OnDestroy();
	}
}

// Post-update (for late update)
void Actor::PreUpdate(float deltaTime)
{
	AddPendingComponents();

	// Post-update all components
	for (const auto& component : m_componentPtrs) {
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
	for (const auto& component : m_componentPtrs) {
		if (component->IsDestroyed()) continue;
		component->Update(deltaTime);
	}
}

// Late update
void Actor::LateUpdate(float deltaTime)
{
	// Late update all components
	std::vector<Component*> destroyedComponents;
	for (const auto& component : m_componentPtrs) {
		if (component->IsDestroyed()) 
		{
			destroyedComponents.push_back(component);
			continue;
		}
		component->LateUpdate(deltaTime);
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

// Add pending components to the main component container
void Actor::AddPendingComponents()
{
	for(auto& pending : m_pendingComponents) {
		auto& instance = pending.instance;
		m_componentPtrs.push_back(instance.get());
		auto& bucket = m_components[pending.typeId];
		bucket.instances.push_back(std::move(instance));
	}

	m_pendingComponents.clear();
}

// Remove components marked for destruction
void Actor::RemoveDestroyedComponents(Component* component)
{
	component->OnDestroy();

	auto mapIt = m_components.find(std::type_index(typeid(*component)));
	if (mapIt != m_components.end()) {
		auto& instances = mapIt->second.instances;
		auto instance = std::find_if(instances.begin(), instances.end(), [component](const std::unique_ptr<Component>& instance) {
			return instance.get() == component;
			});
		if (instance != instances.end()) {
			instances.erase(instance);
		}
	}

	auto ptrIt = std::find(m_componentPtrs.begin(), m_componentPtrs.end(), component);
	if (ptrIt != m_componentPtrs.end()) {
		m_componentPtrs.erase(ptrIt);
	}
}

// Add a child actor to the scene
void Actor::AddChildActorToScene(std::unique_ptr<Actor> child)
{
	if (m_pOwner) {
		m_pOwner->AddActorToPending(std::move(child));
	}
}
