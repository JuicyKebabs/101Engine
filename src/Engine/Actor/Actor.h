#pragma once
#include <DirectXMath.h>
#include <vector>
#include "Engine/Component/Component.h"
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
		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		T* ptr = component.get();
		component->SetOwner(this);
		m_pendingComponents.push_back(std::move(component));
		return ptr;
	}

	// Remove a component from the container
	void RemoveComponent(Component* component){
		component->MarkForDestruction();
	}

	// Get a component of type T from the container by class type
	template<class T>
	T* GetComponentByClass(){
		static_assert(std::is_base_of_v<Component, T>, "GetComponent<T>: T must derive from Component");
		for (const auto& component : m_components) {
			if (auto casted = dynamic_cast<T*>(component.get())) {
				return casted;
			}
		}
		return nullptr;
	}

	// Get all components of type T from the container by class type
	template<class T>
	std::vector<T*> GetComponentsByClass(){
		static_assert(std::is_base_of_v<Component, T>, "GetComponent<T>: T must derive from Component");
		std::vector<T*> result;
		for (const auto& component : m_components) {
			if (auto casted = dynamic_cast<T*>(component.get())) {
				result.push_back(casted);
			}
		}
		return result;
	}

	// Add a child actor
	template<class T, class... Args>
	T* AddChild(Args&&... args) {
		static_assert(std::is_base_of_v<Actor, T>, "AddChild<T>: T must derive from Actor");
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

	std::vector<std::unique_ptr<Component>> m_components;			// Component container
	std::vector<std::unique_ptr<Component>> m_pendingComponents;	// Pending component container

	SceneBase* m_pOwner;	// Pointer to the owning scene
	Transform* m_pTransform;	// Pointer to the Transform component

	Actor* m_pParent = nullptr;		// Pointer to the parent actor (nullptr if no parent)
	std::vector<Actor*> m_children;	// Child actors

private:
	void RemoveDestroyedComponents(Component* component);
	void AddChildActorToScene(std::unique_ptr<Actor> child);
};