#pragma once
#include "Engine/Window/WindowInfo.h"
#include "Engine/Component/Camera.h"
#include "Engine/Core/ComPtr/ComPtr.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Actor/Actor.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Graphics/RenderSystem.h"
#include "Engine/Graphics/CameraSystem.h"
#include "Engine/Graphics/LightTypes.h"
#include "Engine/Physics/CollisionSystem.h"
#include "Engine/Scene/SceneLoader.h"

// Scene base class
// Base class for all scene classes
class SceneBase
{
public:
	static constexpr DirectX::XMFLOAT3 SKY_BOX_SIZE = { 50.0f, 50.0f, 50.0f }; // Skybox size
public:
	SceneBase();	// Constructor
	~SceneBase();	// Destructor

	// Main processing functions
	void Initialize(EngineContext& context);	// Initialization
	void PreUpdate(float deltaTime);			// Pre-update
	void Update(float deltaTime);				// Update
	void LateUpdate(float deltaTime);			// Late update
	void OnRender(EngineContext& context);		// Render
	void Finalize();							// Finalize

	// Add an root actor
	Actor* AddRootActor(Actor* actor) 
	{
		if (!actor) return nullptr;
		actor->SetOwner(this);
		m_addPendingRootActors.push_back(std::unique_ptr<Actor>(actor));
		m_addPendingAllActors.push_back(actor);
		return actor;
	}

	// Add an actor to the scene (can be used for child actors that are not owned by the scene)
	Actor* AddActor(Actor* actor) 
	{
		if (!actor) return nullptr;
		actor->SetOwner(this);
		m_addPendingAllActors.push_back(actor);
		return actor;
	}

	// Get root actors (actors without parents, owned by the scene)
	std::vector<Actor*> GetRootActors() const 
	{
		std::vector<Actor*> rootActorPtrs;
		for (const auto& actor : m_rootActors) {
			rootActorPtrs.push_back(actor.get());
		}
		return rootActorPtrs;
	}	

	// Flush pending actors to the scene
	void FlushPendingActors() 
	{
		// Add pending root actors to the scene
		for (auto& pendingActor : m_addPendingRootActors)
		{
			m_rootActors.push_back(std::move(pendingActor));
		}
		m_addPendingRootActors.clear();
		// Add pending actors to the all actors list(including children)
		for (auto& pendingActor : m_addPendingAllActors)
		{
			m_allActors.push_back(pendingActor);
		}
		m_addPendingAllActors.clear();
	}

	// Get all actors in the scene (including children)
	std::vector<Actor*> GetAllActors() const { return m_allActors; }

	// Getters for systems
	RenderSystem* GetRenderSystem() const { return m_pRenderSystem.get(); }				// Get render system
	CameraSystem* GetCameraSystem() const { return m_pCameraSystem.get(); }				// Get camera system
	CollisionSystem* GetCollisionSystem() const { return m_pCollisionSystem.get(); }	// Get collision system

	// Setters and getters for directional light
	void SetDirectionalLight(const DirectionalLight& light) { m_directionalLight = light; }	// Set directional light
	DirectionalLight GetDirectionalLight() const { return m_directionalLight; }				// Get directional light

private:
	std::vector<std::unique_ptr<Actor>> m_rootActors;			// Root actors (actors without parents, owned by the scene)
	std::vector<Actor*> m_allActors;							// Raw pointer array of all actors in the scene (including children)
	std::vector<std::unique_ptr<Actor>> m_addPendingRootActors;	// Pending root actors to be added (owned by the scene)
	std::vector<Actor*> m_addPendingAllActors;					// Raw pointer array of pending objects to be added (including children)

	std::unique_ptr<RenderSystem> m_pRenderSystem = nullptr;		// Render system
	std::unique_ptr<CameraSystem> m_pCameraSystem = nullptr;		// Camera system
	std::unique_ptr<CollisionSystem> m_pCollisionSystem = nullptr;	// Collision system

protected:
	DirectionalLight m_directionalLight;	// Directional light

private:
	virtual void InitializeOverride(EngineContext& context) = 0;	// Initialization (override in derived class)
};