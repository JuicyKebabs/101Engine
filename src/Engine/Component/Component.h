#pragma once
#include <string>
#include <memory>

// Forward declaration
class Actor;

// Component class 
class Component
{
public:
	Component(const std::string& name = "") : m_name(name) {}
	virtual ~Component() = default;

	void SetOwner(Actor* owner) { m_pOwner = owner; }	// Set the owning actor
	Actor* GetOwner() { return m_pOwner; }				// Get the owning actor

	virtual void OnStart() = 0;						// Called when the component is initialized
	virtual void PreUpdate(float deltaTime) = 0;	// Called every frame before Update to perform pre-update tasks
	virtual void Update(float deltaTime) = 0;		// Called every frame to update the component
	virtual void LateUpdate(float deltaTime) = 0;	// Called every frame after Update to perform late updates
	virtual void OnDestroy() = 0;					// Called when the component is destroyed


	void MarkAsStarted() { m_started = true; }			// Mark the component as started
	bool IsStarted() const { return m_started; }		// Check if the component has been started
	void MarkForDestruction() { m_destroyed = true; }	// Mark the component for destruction
	bool IsDestroyed() const { return m_destroyed; }	// Check if the component is marked for destruction

private:
	Actor* m_pOwner = nullptr;	// Pointer to the owning actor
	std::string m_name;			// Component name (optional, can be used for debugging or identification)
	bool m_started = false;		// Flag to check if OnStart has been called
	bool m_destroyed = false;	// Flag to check if the component is marked for destruction
};