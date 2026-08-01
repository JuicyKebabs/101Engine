#include "Actor.h"
#include "ActorPool.h"
#include "Engine/Engine.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/Behavior.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Scene/ComponentRegistry.h"

// Destructor
Actor::~Actor()
{
}

// Post-update (for late update)
void Actor::PreUpdate(float deltaTime)
{
	AddPendingComponents();

	// Post-update all components
	for (const auto& component : m_componentPtrs) 
	{
		// If the component has not been started, call OnStart and mark it as started
		if (!component->IsStarted()) { 
			component->OnStart(); 
		}

		// Call PostUpdate for each component
		component->PreUpdate(deltaTime);
	}
}

// Update
void Actor::Update(float deltaTime)
{
	// Update all components
	for (const auto& component : m_componentPtrs) { component->Update(deltaTime); }
}

// Late update
void Actor::LateUpdate(float deltaTime)
{
	// Late update all components
	std::vector<Component*> destroyedComponents;
	for (const auto& component : m_componentPtrs) 
	{
		if (component->IsDestroyed()) 
		{
			destroyedComponents.push_back(component);
			continue;
		}
		component->LateUpdate(deltaTime);
	}

	// Remove components marked for destruction
	for (auto& destroyed : destroyedComponents) 
	{
		RemoveDestroyedComponents(destroyed);
	}
}

// Mark as actor as destroyed
void Actor::Destroy()
{
	if (m_destroyed) return;
	if (m_pOwner)
	{
		// SceneBase owns hierarchy policy and keeps Actor/ActorPool state in sync.
		m_pOwner->RemoveActor(this, /*cascadeToChildren=*/true);
		return;
	}

	// An unregistered actor has no pool to notify.
	m_destroyed = true;
}

// Check if actor is destroyed
bool Actor::IsDestroyed()
{
	return m_destroyed;
}

// Called from ActorPool::CollectGarbage before the actor is released.
void Actor::OnDestroy()
{
	// Detach from parent (no alive actor can access this actor anymore)
	SetParentHandle(ActorHandle::Null());

	for (auto& component : m_componentPtrs)
	{
		component->OnDestroy();
	}
	for (auto& pending : m_pendingComponents)
	{
		pending.instance->OnDestroy();
	}
}

Component* Actor::AddComponent(std::unique_ptr<Component> component)
{
	if (!component) return nullptr;

	// Get component policy information
	const std::type_index typeId = std::type_index(typeid(*component));
	const auto* entry = ComponentRegistry::Get().GetEntry(typeId);

	if (!entry)
	{
		DBG("Actor::AddComponent: Component type '%s' is not registered", typeId.name());
		return nullptr;
	}

	// Add component and return the raw pointer to the added component
	return AddComponentInternal(std::move(component), typeId, entry->cardinality, entry->family);
}

Component* Actor::AddComponentInternal(
	std::unique_ptr<Component> component,
	std::type_index typeId,
	ComponentCardinality cardinality,
	ComponentFamily family
)
{
	if (!component) return nullptr;

	// In case of component which is not allowed to be added multiple times
	if (cardinality != ComponentCardinality::Multiple && HasExactComponent(typeId))
	{
		DBG("Actor::AddComponent: Actor '%s' already has component '%s'.", m_name.c_str(), typeId.name());
		return nullptr;
	}

	// In case of component which is not allowed to be added multiple times in the same family
	if (family != ComponentFamily::None && HasComponentFamily(family))
	{
		DBG("Actor::AddComponent: Actor '%s' already has a component in family '%d'.", m_name.c_str(), static_cast<int>(family));
		return nullptr;
	}

	// Set the owner of the component to this actor
	Component* ptr = component.get();
	ptr->SetOwner(this);

	// Add the component to the pending components list for later processing
	PendingComponent pending(std::move(component), typeId);
	m_pendingComponents.push_back(std::move(pending));

	return ptr;
}

void Actor::SetParentHandle(ActorHandle parentHandle)
{
	if (m_parentHandle == parentHandle) return;

	// Detach from current parent, if any.
	if (!m_parentHandle.IsNull() && m_pOwner)
	{
		Actor* oldParent = m_pOwner->ResolveActor(m_parentHandle);
		if (oldParent)
		{
			auto& siblings = oldParent->m_childHandles;
			siblings.erase(
				std::remove(siblings.begin(), siblings.end(), m_handle),
				siblings.end());
		}
	}

	m_parentHandle = parentHandle;

	// Attach to new parent, if any.
	if (!m_parentHandle.IsNull() && m_pOwner)
	{
		Actor* newParent = m_pOwner->ResolveActor(m_parentHandle);
		if (newParent)
		{
			newParent->m_childHandles.push_back(m_handle);
		}
	}
}

Actor* Actor::GetParent() const
{
	if (m_parentHandle.IsNull() || !m_pOwner) return nullptr;
	return m_pOwner->ResolveActor(m_parentHandle);
}

// Get direct child actors (non-recursive)
std::vector<Actor*> Actor::GetDirectChildren() const
{
	std::vector<Actor*> result;
	if (!m_pOwner) return result;

	for (const auto& handle : m_childHandles)
	{
		if (Actor* child = m_pOwner->ResolveActor(handle))
		{
			result.push_back(child);
		}
	}
	return result;
}

// Check if the actor has a component by name
bool Actor::HasComponentByName(const std::string& name) const
{
	if (name.empty()) return false;

	for (const auto& typeId : GetComponentsTypeIds()) 
	{
		if(ComponentRegistry::Get().GetNameByTypeIndex(typeId) == name) 
		{
			return true;
		}
	}

	return false;
}

bool Actor::HasExactComponent(std::type_index typeId) const
{
	auto it = m_components.find(typeId);

	// Serach in the main component container
	if (it != m_components.end() && !it->second.instances.empty()) return true;
	
	// Search in the pending components list
	for (const auto& pending : m_pendingComponents)
	{
		if (pending.typeId == typeId) return true;
	}

	return false;
}

bool Actor::HasComponentFamily(ComponentFamily family) const
{
	return CountComponentFamily(family) > 0;
}

size_t Actor::CountComponentFamily(ComponentFamily family) const
{
	if (family == ComponentFamily::None) return 0;

	size_t count = 0;

	// Count in the main component container
	for (const auto& [typeId, bucket] : m_components)
	{
		if (const auto* entry = ComponentRegistry::Get().GetEntry(typeId))
		{
			if (entry->family == family)
			{
				count += bucket.instances.size();
			}
		}
	}

	// Count in the pending components list
	for (const auto& pending : m_pendingComponents)
	{
		if (const auto* entry = ComponentRegistry::Get().GetEntry(pending.typeId))
		{
			if (entry->family == family)
			{
				count++;
			}
		}
	}

	return count;
}

// Get child actors by recursively traversing the hierarchy
std::vector<Actor*> Actor::GetChildren() const
{
	std::vector<Actor*> result;

	// Get direct children and recursively get their children
	for (Actor* child : GetDirectChildren())
	{
		result.push_back(child);
		auto grandChildren = child->GetChildren();
		result.insert(result.end(), grandChildren.begin(), grandChildren.end());
	}

	return result;
}

// Update world transform of this actor and all child actors;
void Actor::FlushTransform()
{
	auto pTransform = GetComponentByClass<Transform>();
	if (pTransform) pTransform->UpdateGeometry();

	for (Actor* child : GetDirectChildren())
	{
		child->FlushTransform();
	}
}

// Update collider transforms to match the current world transform of the actor
void Actor::FlushColliderTransforms()
{
	auto colliders = GetComponentsByClass<Collider>();
	for (auto& collider : colliders) collider->Flush();

	for (Actor* child : GetDirectChildren())
	{
		child->FlushColliderTransforms();
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
	if (mapIt != m_components.end()) 
	{
		auto& instances = mapIt->second.instances;
		auto instance = std::find_if(instances.begin(), instances.end(), [component](const std::unique_ptr<Component>& instance) {
			return instance.get() == component;
			});
		if (instance != instances.end()) 
		{
			instances.erase(instance);
		}
	}

	auto ptrIt = std::find(m_componentPtrs.begin(), m_componentPtrs.end(), component);
	if (ptrIt != m_componentPtrs.end()) 
	{
		m_componentPtrs.erase(ptrIt);
	}
}

Actor* Actor::AddChild(std::unique_ptr<Actor> child)
{
	if (!m_pOwner || !child) return nullptr;
	return m_pOwner->AddChildActor(std::move(child), m_handle);
}

bool Actor::ReplaceTransformComponent(std::unique_ptr<Transform> transform)
{
	if (!transform) return false;

	transform->SetOwner(this);

	const std::type_index transformType = std::type_index(typeid(Transform));
	const std::type_index rectTransformType = std::type_index(typeid(RectTransform));
	const std::type_index newType = std::type_index(typeid(*transform));

	// Serch for pending transform component
	for (auto& pending : m_pendingComponents)
	{
		if (pending.typeId != transformType && pending.typeId != rectTransformType) continue;
		pending.instance = std::move(transform);
		pending.typeId = newType;
		return true;
	}

	// serch for existing transform component
	for (const std::type_index typeId : { transformType, rectTransformType })
	{
		auto bucketIt = m_components.find(typeId);
		if (bucketIt == m_components.end() || bucketIt->second.instances.empty()) continue;

		// Get the first instance of the transform component (there should be only one)
		auto& instances = bucketIt->second.instances;

		std::unique_ptr<Component> oldTransform = std::move(instances.front());

		// Store pointer to the old and new transform
		Component* oldTransformPtr = oldTransform.get();
		Component* newTransformPtr = transform.get();

		auto pointerIt = std::find(m_componentPtrs.begin(), m_componentPtrs.end(), oldTransformPtr);

		if (newType == typeId)
		{// In case of the replacing to the same type
			// Just replace the existing transform with the new one
			instances.front() = std::move(transform);
			if (pointerIt != m_componentPtrs.end()) *pointerIt = newTransformPtr;

			return true;
		}

		// In case of replacing to a different type (Transform <-> RectTransform)
		// Remove the old transform from the main component container
		m_components.erase(bucketIt);

		// Add the new transform to its bucket
		auto& newBucket = m_components[newType];
		newBucket.instances.push_back(std::move(transform));

		if (pointerIt != m_componentPtrs.end())
		{
			// Replace the old iteration pointer with the new transform
			*pointerIt = newTransformPtr;
		}

		return true;
	}

	DBG("Actor::ReplaceTransformComponent: Actor '%s' has no Transform-family component.", m_name.c_str());
	return false;
}
