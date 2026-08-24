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
	AttachPendingComponents();

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
		component->OnDetach();
		component->OnDestroy();
	}
	for (auto& pending : m_pendingComponents)
	{
		pending.instance->OnDetach();
		pending.instance->OnDestroy();
	}
}

Component* Actor::AddComponent(std::unique_ptr<Component> component)
{
	if (!component) return nullptr;

	const std::type_index typeId = std::type_index(typeid(*component));

	if (!ComponentRegistry::Get().GetPolicy(typeId))
	{
		DBG("Actor::AddComponent: Component type '%s' is not registered", typeId.name());
		return nullptr;
	}

	// Add component and return the raw pointer to the added component
	return AddComponentInternal(std::move(component), typeId);
}

Component* Actor::AddComponentInternal(
	std::unique_ptr<Component> component,
	std::type_index typeId
)
{
	if (!component) return nullptr;

	// Check if it is allowed to add this component type based on its cardinality and family constraints
	if (!CanAddComponent(typeId))
	{
		DBG("Actor::AddComponent: Component type '%s' is not allowed on Actor '%s'.", typeId.name(), m_name.c_str());
		return nullptr;
	}

	// Set the owner of the component to this actor
	Component* ptr = component.get();
	ptr->SetOwner(this);

	// Add the component to the pending components list for later processing
	PendingComponent pending(std::move(component), typeId);
	m_pendingComponents.push_back(std::move(pending));

	// Notify the owner scene that a component has been added to this actor
	// in order to reapply UI hierarchy constraints if necessary (Canvas/UIRenderer)
	if (m_pOwner)
	{
		m_pOwner->OnActorComponentAdded(this, ptr);
		AttachPendingComponents();
	}

	return ptr;
}

Component* Actor::AddComponentImmediate(std::unique_ptr<Component> component, std::size_t occurrenceIndex)
{
	if (!component) return nullptr;

	const std::type_index typeId = typeid(*component);

	// Check if it's allowed to add given component type
	// based on its cardinality and family constraints
	if (!CanAddComponent(typeId))
	{
		return nullptr;
	}

	// Check if the given occurrence index is within
	// the valid range of existing components of the same type
	const std::size_t componentCount = GetComponentsByExactType(typeId).size();

	if (occurrenceIndex > componentCount) return nullptr;

	// Normalize storage of pending components before adding the new component
	AttachPendingComponents();

	// Get components of the same type
	auto& instances = m_components[typeId].instances;

	Component* componentPtr = component.get();
	componentPtr->SetOwner(this);

	Component* nextComponent = nullptr;
	auto pointerIt = m_componentPtrs.end();

	// Get the component at the specified occurrence index
	// to insert given component before it
	if (occurrenceIndex < instances.size())
	{
		nextComponent = instances[occurrenceIndex].get();

		// Find the next component in the iteration vector (m_componentPtrs)
		// to check if it exists and to get the iterator for insertion
		pointerIt = std::find(
			m_componentPtrs.begin(),
			m_componentPtrs.end(),
			nextComponent
		);

		if (pointerIt == m_componentPtrs.end()) return nullptr;
	}

	// Add component to the appropriate position in the type based instances vector
	instances.insert(instances.begin() + occurrenceIndex, std::move(component));

	// Add to the iteration vector (m_componentPtrs) while maintaining component order
	if (pointerIt != m_componentPtrs.end())
	{// If there is a component behind the row, insert the new component before it
		m_componentPtrs.insert(pointerIt, componentPtr);
	}
	else
	{// If there is no component behind the row
		m_componentPtrs.push_back(componentPtr);
	}

	return componentPtr;
}

bool Actor::RemoveComponentImmediate(Component* component)
{
	if (!component ||
		component->GetOwner() != this ||
		component->IsDestroyed())
	{
		return false;
	}

	const std::type_index typeId = typeid(*component);
	const auto policy = ComponentRegistry::Get().GetPolicy(typeId);

	// Don't remove components that are required to be unique on the actor
	if (!policy || policy->cardinality == ComponentCardinality::UniqueRequired)
	{
		return false;
	}

	const auto components = GetComponentsByExactType(typeId);

	if (std::find(
		components.begin(),
		components.end(),
		component) == components.end())
	{
		return false;
	}

	AttachPendingComponents();

	// Get bucket of components of the same type
	auto bucketIt = m_components.find(typeId);

	if (bucketIt == m_components.end()) return false;

	// Get the instances vector of the same type
	auto& instances = bucketIt->second.instances;

	// Get given component from the instances vector of the same type
	auto instanceIt = std::find_if(
		instances.begin(), instances.end(),
		[component](const std::unique_ptr<Component>& instance)
		{
			return instance.get() == component;
		}
	);

	if (instanceIt == instances.end()) return false;

	// Get given component from the iteration vector (m_componentPtrs)
	auto pointerIt = std::find(
		m_componentPtrs.begin(), m_componentPtrs.end(),
		component
	);

	if (pointerIt == m_componentPtrs.end()) return false;

	// Detach the component from scene systems before destroying its instance.
	component->OnDetach();
	component->OnDestroy();

	// Remove the component from both the iteration vector and the instances vector
	m_componentPtrs.erase(pointerIt);
	instances.erase(instanceIt);
	
	// Remove the bucket if there are no more instances of this component type
	if (instances.empty())
	{
		m_components.erase(bucketIt);
	}

	return true;
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

bool Actor::CanAddComponent(std::type_index typeId) const
{
	const auto policy = ComponentRegistry::Get().GetPolicy(typeId);
	if (!policy) return false;

	// Components which are not Multiple cannot be added
	// when the same concrete type already exists.
	if (policy->cardinality != ComponentCardinality::Multiple && HasExactComponent(typeId))
	{
		return false;
	}

	// Only one component from the same family can exist on an Actor.
	if (policy->family != ComponentFamily::None && HasComponentFamily(policy->family))
	{
		return false;
	}

	return true;
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
		if (const auto policy = ComponentRegistry::Get().GetPolicy(typeId))
		{
			if (policy->family == family)
			{
				count += bucket.instances.size();
			}
		}
	}

	// Count in the pending components list
	for (const auto& pending : m_pendingComponents)
	{
		if (const auto policy = ComponentRegistry::Get().GetPolicy(pending.typeId))
		{
			if (policy->family == family)
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
void Actor::AttachPendingComponents()
{
	if (!m_pOwner) return;

	for (auto& pending : m_pendingComponents)
	{
		auto& instance = pending.instance;
		Component* component = instance.get();

		m_componentPtrs.push_back(component);

		auto& bucket = m_components[pending.typeId];
		bucket.instances.push_back(std::move(instance));

		component->OnAttach();
	}

	m_pendingComponents.clear();
}
// Remove components marked for destruction
void Actor::RemoveDestroyedComponents(Component* component)
{
	component->OnDetach();
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

void Actor::AttachComponents()
{
	if (!m_pOwner) return;

	AttachPendingComponents();

	for (Component* component : m_componentPtrs)
	{
		if (component) component->OnAttach();
	}
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
		oldTransformPtr->OnDetach();

		auto pointerIt = std::find(m_componentPtrs.begin(), m_componentPtrs.end(), oldTransformPtr);

		if (newType == typeId)
		{// In case of the replacing to the same type
			// Just replace the existing transform with the new one
			instances.front() = std::move(transform);
			if (pointerIt != m_componentPtrs.end()) *pointerIt = newTransformPtr;
			newTransformPtr->OnAttach();

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

		newTransformPtr->OnAttach();

		return true;
	}

	DBG("Actor::ReplaceTransformComponent: Actor '%s' has no Transform-family component.", m_name.c_str());
	return false;
}
