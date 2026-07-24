#pragma once
#include <vector>
#include <typeindex>
#include <unordered_map>
#include "Engine/Component/Component.h"
#include "Engine/Component/ComponentPolicy.h"
#include "Engine/Component/Transform.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Actor/ActorHandle.h"
#include "Engine/Core/GUID/Guid.h"

//-----------------------------------------------------------------------------
// Actor class
// This class represents all entities in the game world. 
// It contains components that define its behavior and properties,.
// This can have a hierarchical relationship with other actors (parent-child).
//-----------------------------------------------------------------------------

// Forward declaration
class ActorFactory;
class Renderer;
class TextureManager;
class MeshManager;
class SceneBase;

// Actor Class
// This defines the all elements that exist in the game world
class Actor
{
public:
	struct InitDesc
	{
		bool isActive;
		TagId tag;
		std::string name;
		InitDesc(bool isActive = true, TagId tag = TAG_NONE, std::string name = "Actor"
		) : isActive(isActive), tag(tag), name(name) {}
	};

	struct ComponentBucket {
		std::vector<std::unique_ptr<Component>> instances;
	};

	struct PendingComponent {
		std::unique_ptr<Component> instance;
		std::type_index typeId;
	};

public:
	~Actor();	// Destructor

	void PreUpdate(float deltaTime);	// Pre-update
	void Update(float deltaTime);		// Update
	void LateUpdate(float deltaTime);	// Late update

	void Destroy();		// Mark actor as destroyed
	bool IsDestroyed();	// Check if actor is destroyed
	void OnDestroy();

	// Setters
	void SetHandle(ActorHandle handle) { m_handle = handle; }
	void SetOwner(SceneBase* ownerScene) { m_pOwner = ownerScene; }
	void SetTag(TagId tag) { m_tag = tag; }
	void SetActive(bool isActive) { m_isActive = isActive; }

	// Getters
	const Guid& GetGuid() const { return m_guid; }
	ActorHandle GetHandle() const { return m_handle; }
	SceneBase* GetOwner() const { return m_pOwner; }
	TagId GetTag() const { return m_tag; }
	bool IsActive() const { return m_isActive; }
	bool IsInitialized() const { return m_isInitialized; }
	const std::string& GetName() const { return m_name; }

	// Parent handle
	void SetParentHandle(ActorHandle parent);
	ActorHandle GetParentHandle() const { return m_parentHandle; }

	// Get the parent actor pointer (via ActorPool)
	Actor* GetParent() const;

	// Get all descendant actors
	std::vector<ActorHandle> GetChildrenHandles() const { return m_childHandles; }
	std::vector<Actor*> GetChildren() const;

	// Get only the direct children of this actor
	std::vector<Actor*> GetDirectChildren() const;

	// Add a component of type T to the container
	template<class T, class... Args>
	T* AddComponent(Args&&... args)
	{
		static_assert(std::is_base_of_v<Component, T>, "AddComponent<T>: T must derive from Component");

		using Policy = ComponentPolicy<T>;
		Policy policy;
		if (policy.cardinality != ComponentCardinality::Multiple) 
		{
			if (HasComponent<T>()) return GetComponentByClass<T>();
		}

		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		T* ptr = component.get();
		component->SetOwner(this);
		PendingComponent pending{ std::move(component), std::type_index(typeid(T)) };
		m_pendingComponents.push_back(std::move(pending));
		return ptr;
	}

	// Add a pre-created component instance to the container.
	// Used by ComponentRegistry::AddToActor, where the concrete type is only
	// known at runtime (created via a Factory function returning Component*).
	//
	// NOTE (recovery): in the lost Editor work, a ComponentPolicy-based
	// duplicate-prevention check (for UniqueRequired-style components such as
	// Transform/Camera) was reportedly added to this overload so that
	// InspectorPanel's "Add Component" text field couldn't attach duplicates.
	// The exact implementation wasn't captured in chat, so this restored
	// version is the pre-policy-check baseline (known working, used by
	// SceneLoader/SceneWriter). Re-add the duplicate check here if desired.
	Component* AddComponent(std::unique_ptr<Component> component)
	{
		if (!component) return nullptr;
		Component* ptr = component.get();
		ptr->SetOwner(this);
		PendingComponent pending{ std::move(component), std::type_index(typeid(*ptr)) };
		m_pendingComponents.push_back(std::move(pending));
		return ptr;
	}

	// Check if the container has a component of type T
	template<class T>
	bool HasComponent() const 
	{
		static_assert(std::is_base_of_v<Component, T>, "HasComponent<T>: T must derive from Component");

		bool result = false;

		auto it = m_components.find(GetComponentTypeId<T>());
		 if (it != m_components.end() && !it->second.instances.empty()) 
		 {
			 for (const auto& instance : it->second.instances)
			 {
				 if (static_cast<T*>(instance.get()))
				 {
					 result = true;
				 }
			 }
		 }

		 for(auto& pending : m_pendingComponents) 
		 {
			 if (pending.typeId == GetComponentTypeId<T>()) 
			 {
				 if (static_cast<T*>(pending.instance.get())) 
				 {
					 result = true;
				 }
			 }
		 }

		return result;
	}

	// Check if the container has a component by name
	bool HasComponentByName(const std::string& name) const;

	// Get a component of type T from the container by class type
	template <class T>
	std::type_index GetComponentTypeId() const
	{
		static_assert(std::is_base_of_v<Component, T>, "GetComponentTypeId<T>: T must derive from Component");
		return std::type_index(typeid(T));
	}

	// Remove a component of type T from the container by class type
	template<class T>
	void RemoveComponentByClass()
	{
		static_assert(std::is_base_of_v<Component, T>, "RemoveComponentByClass<T>: T must derive from Component");
		using Policy = ComponentPolicy<T>;
		Policy policy;
		if(policy.cardinality == ComponentCardinality::UniqueRequired)
		{
			static_assert(policy.cardinality != ComponentCardinality::UniqueRequired, "RemoveComponentByClass<T>: Components of this type cannot be removed");
			return;
		}
		
		auto typeId = GetComponentTypeId<T>();
		auto bucket = m_components.find(typeId);
		if (bucket != m_components.end() && !bucket->second.instances.empty())
		{
			for (const auto& instance : bucket->second.instances)
			{
				if (auto casted = static_cast<T*>(instance.get()))
				{
					casted->MarkForDestruction();
				}
			}
		}
		for (auto& pending : m_pendingComponents)
		{
			if (pending.typeId == typeId)
			{
				if (auto casted = static_cast<T*>(pending.instance.get()))
				{
					casted->MarkForDestruction();
				}
			}
		}
	}

	// Get a component of type T from the container by class type
	template<class T>
	T* GetComponentByClass(){
		static_assert(std::is_base_of_v<Component, T>, "GetComponent<T>: T must derive from Component");
		
		// Exact type match search
		auto typeId = GetComponentTypeId<T>();
		auto it = m_components.find(typeId);
		if (it != m_components.end() && !it->second.instances.empty())
		{
			for (const auto& instance : it->second.instances)
			{
				if (auto casted = static_cast<T*>(instance.get()))
				{
					return casted;
				}
			}
		}
		for (auto& pending : m_pendingComponents)
		{
			if (pending.typeId == typeId)
			{
				if (auto casted = static_cast<T*>(pending.instance.get()))
				{
					return casted;
				}
			}
		}

		// Inheritance search
		for (auto& [id, bucket] : m_components)
		{
			if (id == typeId) continue;
			for(const auto& instance : bucket.instances)
			{
				if (auto casted = dynamic_cast<T*>(instance.get()))
				{
					return casted;
				}
			}
		}
		for(auto& pending : m_pendingComponents)
		{
			if (pending.typeId == typeId) continue;
			if (auto casted = dynamic_cast<T*>(pending.instance.get()))
			{
				return casted;
			}
		}

		return nullptr;
	}

	// Get all components of type T from the container by class type
	template<class T>
	std::vector<T*> GetComponentsByClass(){
		static_assert(std::is_base_of_v<Component, T>, "GetComponent<T>: T must derive from Component");
		
		// Exact type match search
		auto it = m_components.find(GetComponentTypeId<T>());
		std::vector<T*> result;
		if(it != m_components.end() && !it->second.instances.empty())
		{
			for (const auto& instance : it->second.instances)
			{
				if (auto casted = static_cast<T*>(instance.get()))
				{
					result.push_back(casted);
				}
			}
		}
		for(auto& pending : m_pendingComponents)
		{
			 if (pending.typeId == GetComponentTypeId<T>())
			 {
				 if (auto casted = static_cast<T*>(pending.instance.get())) 
				 {
					 result.push_back(casted);
				 }
			 }
		}

		// Inheritance search
		for (auto& [id, bucket] : m_components)
		{
			if (id == GetComponentTypeId<T>()) continue;
			for(const auto& instance : bucket.instances)
			{
				if (auto casted = dynamic_cast<T*>(instance.get())) 
				{
					result.push_back(casted);
				}
			}
		}
		for(auto& pending : m_pendingComponents)
		{
			if (pending.typeId == GetComponentTypeId<T>()) continue;
			if (auto casted = dynamic_cast<T*>(pending.instance.get()))
			{
				result.push_back(casted);
			}
		}
		return result;
	}

	// Get all component type IDs currently attached to this actor
	std::vector<std::type_index> GetComponentsTypeIds() const
	{
		std::vector<std::type_index> result;
		for(auto& [typeId, bucket] : m_components)
		{
			if (!bucket.instances.empty())
			{
				result.push_back(typeId);
			}
		}
		for(auto& pending : m_pendingComponents)
		{
			if (std::find(result.begin(), result.end(), pending.typeId) == result.end())
			{
				result.push_back(pending.typeId);
			}
		}
		return result;
	}

	std::vector<Component*> GetAllComponents()
	{
		std::vector<Component*> result;
		for (auto& [typeId, bucket] : m_components)
		{
			for (auto& instance : bucket.instances)
			{
				if (instance && !instance->IsDestroyed())
				{
					result.push_back(instance.get());
				}
			}
		}
		for (auto& pending : m_pendingComponents)
		{
			if (pending.instance && !pending.instance->IsDestroyed())
			{
				result.push_back(pending.instance.get());
			}
		}
		return result;
	}

	// Add a child actor
	Actor* AddChild(std::unique_ptr<Actor> child);

	void FlushTransform();			// Update world transform of this actor and all child actors;
	void FlushColliderTransforms(); // Update collider transforms of this actor and all child actors

private:
	// Flags
	bool m_isActive = false;		// Active flag
	bool m_destroyed = false;		// Destroyed flag
	bool m_isInitialized = false;	// Initialization flag (whether Init function has been called at least once)

	// Component storage
	std::unordered_map<std::type_index, ComponentBucket> m_components;	// Main storage of components, organized by type
	std::vector<Component*> m_componentPtrs;							// For iteration
	std::vector<PendingComponent> m_pendingComponents;					// Pending additions

	// Hierarchy
	Guid m_guid;								// Unique identifier for this actor
	ActorHandle m_handle;						// Unique handle for this actor
	SceneBase* m_pOwner = nullptr;				// Owning scene pointer
	ActorHandle m_parentHandle = {};			// Parent actor handle
	std::vector<ActorHandle> m_childHandles;	// Child actor handles

	// Other
	TagId m_tag = TAG_NONE; // Object tag
	std::string m_name;		// Actor name (for debugging and editor)

private:
	void AddPendingComponents();
	void RemoveDestroyedComponents(Component* component);
	void MarkForDestruction() { m_destroyed = true; }

private:
	// Hide default constructor and initialization from public, enforce usage of Init function and ActorFactory for creation
	Actor() = default;			
	void Init(const InitDesc& desc)
	{
		m_isActive = desc.isActive;
		m_tag = desc.tag;
		m_name = desc.name;
		m_isInitialized = true;
	}

	void SetGuid(const Guid& guid) { m_guid = guid; }	// Set the actor's GUID (used by ActorFactory)

	friend class ActorFactory;	// Allow ActorFactory to access private constructor
	friend class SceneBase;		// SceneBase coordinates hierarchy-aware destruction
};
