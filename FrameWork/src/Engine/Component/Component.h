#pragma once
#include <string>
#include <memory>

// Forward declaration
class Actor;
struct EngineContext;

// Component class 
class Component
{

public:
	Component() = default;
	~Component() = default;

	void OnStart() { OnStartOverride(); MarkAsStarted(); }													// Call OnStartOverride if not already started
	void PreUpdate(float deltaTime) { if (IsStarted() && !IsDestroyed()) PreUpdateOverride(deltaTime); }	// Call PreUpdateOverride if started and not destroyed
	void Update(float deltaTime) { if (IsStarted() && !IsDestroyed()) UpdateOverride(deltaTime); }			// Call UpdateOverride if started and not destroyed
	void LateUpdate(float deltaTime) { if (IsStarted() && !IsDestroyed()) LateUpdateOverride(deltaTime); }	// Call LateUpdateOverride if started and not destroyed
	void OnDestroy() { if (!IsDestroyed()) OnDestroyOverride(); MarkForDestruction(); }						// Call OnDestroyOverride if not already destroyed

	void SetOwner(Actor* owner) { m_pOwner = owner; }			// Set the owning actor
	Actor* GetOwner() { return m_pOwner; }						// Get the owning actor
	void SetName(const std::string& name) { m_name = name; }	// Set the component name
	const std::string& GetName() const { return m_name; }		// Get the component name

	void MarkAsStarted() { m_started = true; }					// Mark the component as started
	bool IsStarted() const { return m_started; }				// Check if the component has been started
	void MarkForDestruction() { m_destroyed = true; }			// Mark the component for destruction
	bool IsDestroyed() const { return m_destroyed; }			// Check if the component is marked for destruction

protected:
	EngineContext* GetEngineContext() const;	// Get the engine context from the owning actor's scene

private:
	Actor* m_pOwner = nullptr;		// Pointer to the owning actor
	std::string m_name;				// Component name (optional, can be used for debugging or identification)
	bool m_started = false;			// Flag to check if OnStart has been called
	bool m_destroyed = false;		// Flag to check if the component is marked for destruction

private:
	virtual void OnStartOverride() = 0;						// Called when the component is initialized
	virtual void PreUpdateOverride(float deltaTime) = 0;	// Called every frame before Update to perform pre-update tasks
	virtual void UpdateOverride(float deltaTime) = 0;		// Called every frame to update the component
	virtual void LateUpdateOverride(float deltaTime) = 0;	// Called every frame after Update to perform late updates
	virtual void OnDestroyOverride() = 0;					// Called when the component is destroyed
};