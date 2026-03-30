#pragma once
#include <vector>
#include <typeindex>
#include <unordered_map>
#include "Engine/Component/Component.h"
#include "Engine/Component/ComponentPolicy.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/Utility/SharedStruct.h"

// Forward declaration
class Renderer;
class TextureManager;
class MeshManager;
class SceneBase;

// Actor Class
// This defines the all elements that exist in the game world
class Actor
{
public:
	struct ComponentBucket {
		std::vector<std::unique_ptr<Component>> instances;
	};

	struct PendingComponent {
		std::unique_ptr<Component> instance;
		std::type_index typeId;
	};

public:
	Actor(	// Constructor
		Vector3 position = Vector3{ 0.0f, 0.0f, 0.0f },	// Position
		Vector3 rotation = Vector3{ 0.0f, 0.0f, 0.0f },	// Rotation
		Vector3 scale = Vector3{ 1.0f, 1.0f, 1.0f },	// Scale
		bool isActive = true,							// Active flag
		ACTOR_TAG tag = ACTOR_TAG::NONE					// Object tag
	);
	~Actor();	// Destructor

	void PreUpdate(float deltaTime);	// Pre-update
	void Update(float deltaTime);		// Update
	void LateUpdate(float deltaTime);	// Late update

	void Destroy();		// Mark actor as destroyed
	bool IsDestroyed();	// Check if actor is destroyed

	// Setters
	void SetOwner(SceneBase* ownerScene) { m_pOwner = ownerScene; }	// Set owning scene
	void SetTag(ACTOR_TAG tag) { m_tag = tag; }						// Set object tag
	void SetActive(bool isActive) { m_isActive = isActive; }		// Set active flag
	void SetParent(Actor* parent) { m_pParent = parent; }			// Set parent actor

	// Getters
	SceneBase* GetOwner() const { return m_pOwner; }				// Get owning scene
	ACTOR_TAG GetTag() const { return m_tag; }						// Get object tag
	bool IsActive() const { return m_isActive; }					// Get active status
	Actor* GetParent() const { return m_pParent; }					// Get parent actor
	std::vector<Actor*> GetChildren() const { return m_children; }	// Get child actors

	// Add a component of type T to the container
	template<class T, class... Args>
	T* AddComponent(Args&&... args){
		static_assert(std::is_base_of_v<Component, T>, "AddComponent<T>: T must derive from Component");

		using Policy = ComponentPolicy<T>;
		Policy policy;
		if (policy.cardinality != ComponentCardinality::Multiple) {
			if (HasComponent<T>()) {
				//static_assert(HasComponent<T>(), "AddComponent<T>: A component of this type already exists and multiple instances are not allowed");
				return GetComponentByClass<T>();
			}
		}

		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		T* ptr = component.get();
		component->SetOwner(this);
		PendingComponent pending{ std::move(component), std::type_index(typeid(T)) };
		m_pendingComponents.push_back(std::move(pending));
		return ptr;
	}

	template<class T>
	bool HasComponent() const {
		static_assert(std::is_base_of_v<Component, T>, "HasComponent<T>: T must derive from Component");

		bool result = false;

		auto it = m_components.find(GetComponentTypeId<T>());
		 if (it != m_components.end() && !it->second.instances.empty()) {
			 for (const auto& instance : it->second.instances) {
				 if (static_cast<T*>(instance.get())) {
					 result = true;
				 }
			 }
		 }

		 for(auto& pending : m_pendingComponents) {
			 if (pending.typeId == GetComponentTypeId<T>()) {
				 if (static_cast<T*>(pending.instance.get())) {
					 result = true;
				 }
			 }
		 }

		return result;
	}

	template <class T>
	std::type_index GetComponentTypeId() const {
		static_assert(std::is_base_of_v<Component, T>, "GetComponentTypeId<T>: T must derive from Component");
		return std::type_index(typeid(T));
	}

	template<class T>
	void RemoveComponentByClass() {
		static_assert(std::is_base_of_v<Component, T>, "RemoveComponentByClass<T>: T must derive from Component");
		using Policy = ComponentPolicy<T>;
		Policy policy;
		if(policy.cardinality == ComponentCardinality::UniqueRequired) {
			static_assert(policy.cardinality != ComponentCardinality::UniqueRequired, "RemoveComponentByClass<T>: Components of this type cannot be removed");
			return;
		}
		
		auto typeId = GetComponentTypeId<T>();
		auto bucket = m_components.find(typeId);
		if (bucket != m_components.end() && !bucket->second.instances.empty()) {
			for (const auto& instance : bucket->second.instances) {
				if (auto casted = static_cast<T*>(instance.get())) {
					casted->MarkForDestruction();
				}
			}
		}
		for (auto& pending : m_pendingComponents) {
			if (pending.typeId == typeId) {
				if (auto casted = static_cast<T*>(pending.instance.get())) {
					casted->MarkForDestruction();
				}
			}
		}
	}

	// Get a component of type T from the container by class type
	template<class T>
	T* GetComponentByClass(){
		static_assert(std::is_base_of_v<Component, T>, "GetComponent<T>: T must derive from Component");
		auto typeId = GetComponentTypeId<T>();
		auto it = m_components.find(typeId);
		if (it != m_components.end() && !it->second.instances.empty()) {
			for (const auto& instance : it->second.instances) {
				if (auto casted = static_cast<T*>(instance.get())) {
					return casted;
				}
			}
		}
		for (auto& pending : m_pendingComponents) {
			if (pending.typeId == typeId) {
				if (auto casted = static_cast<T*>(pending.instance.get())) {
					return casted;
				}
			}
		}
		return nullptr;
	}

	// Get all components of type T from the container by class type
	template<class T>
	std::vector<T*> GetComponentsByClass(){
		static_assert(std::is_base_of_v<Component, T>, "GetComponent<T>: T must derive from Component");
		auto it = m_components.find(GetComponentTypeId<T>());
		std::vector<T*> result;
		if(it != m_components.end() && !it->second.instances.empty()) {
			for (const auto& instance : it->second.instances) {
				if (auto casted = static_cast<T*>(instance.get())) {
					result.push_back(casted);
				}
			}
		}
		for(auto& pending : m_pendingComponents) {
			 if (pending.typeId == GetComponentTypeId<T>()) {
				 if (auto casted = static_cast<T*>(pending.instance.get())) {
					 result.push_back(casted);
				 }
			 }
		}
		return result;
	}

	// Add a child actor
	template<class T, class... Args>
	T* AddChild(Args&&... args) {
		static_assert(std::is_base_of_v<Actor, T>, "AddChild<T>: T must derive from Actor");
		if (!m_pOwner) return nullptr;
		auto child = std::make_unique<T>(std::forward<Args>(args)...);
		T* ptr = child.get();
		ptr->SetParent(this);
		m_children.push_back(ptr);
		AddChildActorToScene(std::move(child));
		return ptr;
	}

	Transform* GetTransform() const { return m_pTransform; }	// Get Transform component

	void FlushTransform();	// Update world transform of this actor and all child actors;

private:
	bool m_isActive = false;	// Active flag
	bool m_destroyed = false;	// Flag to check if the actor is marked for destruction

	ACTOR_TAG m_tag = ACTOR_TAG::NONE; // Object tag

	std::unordered_map<std::type_index, ComponentBucket> m_components;	// Component container organized by type
	std::vector<Component*> m_componentPtrs;							// Component pointer container for easy iteration
	std::vector<PendingComponent> m_pendingComponents;					// Pending component container

	SceneBase* m_pOwner = nullptr;		// Pointer to the owning scene
	Transform* m_pTransform = nullptr;	// Pointer to the Transform component

	Actor* m_pParent = nullptr;		// Pointer to the parent actor (nullptr if no parent)
	std::vector<Actor*> m_children;	// Child actors

private:
	void AddPendingComponents();
	void RemoveDestroyedComponents(Component* component);
	void AddChildActorToScene(std::unique_ptr<Actor> child);
};