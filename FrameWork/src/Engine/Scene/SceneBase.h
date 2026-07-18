#pragma once
#include "Engine/Window/WindowInfo.h"
#include "Engine/Component/Camera.h"
#include "Engine/Core/ComPtr/ComPtr.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorPool.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Graphics/RenderSystem.h"
#include "Engine/Graphics/CameraSystem.h"
#include "Engine/Graphics/LightTypes.h"
#include "Engine/Physics/CollisionSystem.h"


//----------------------------------------------------------------------------------------
// SceneBase class
// This class represents a base of scene
// Scene has a list of root actors and all actors in the scene
// This also has systems which are used to process the core functionalities of the scene
//----------------------------------------------------------------------------------------

class SceneManager;	// Forward declaration of SceneManager class

// Scene base class
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

	// Add an actor to the scene
	Actor* AddRootActor(std::unique_ptr<Actor> actor)
	{
		if (!actor) return nullptr;
		actor->SetOwner(this);
		ActorHandle handle = m_actorPool.Register(std::move(actor));
		return m_actorPool.Resolve(handle);
	}

	// Add a child actor to the scene
	// Called by Actor::AddChildActor to add a child actor to the scene
	Actor* AddChildActor(std::unique_ptr<Actor> actor, ActorHandle parentHandle)
	{
		if (!actor) return nullptr;
		actor->SetOwner(this);
		ActorHandle handle = m_actorPool.Register(std::move(actor));
		Actor* ptr = m_actorPool.Resolve(handle);
		if (ptr) ptr->SetParentHandle(parentHandle);
		return ptr;
	}

	// Remove an actor from the scene (mark it for destruction)
	// Actual release happens at the end of the LateUpdate via ActorPool::CollectGarbage
	void RemoveActor(Actor* actor, bool cascadeToChildren = true)
	{
		if (!actor) return;

		if (cascadeToChildren)
		{
			// Recursively remove all children of the actor
			auto children = actor->GetDirectChildren();
			for (auto* child : children)
			{
				RemoveActor(child, true);
			}
		}

		actor->Destroy();
	}

	// Remove an actor from the scene by name
	void RemoveActor(const std::string& name)
	{
		std::vector<Actor*> targets;
		m_actorPool.ForEach([&](Actor* actor) {
			if (actor->GetName() == name)
			{
				targets.push_back(actor);
			}
			});

		for (Actor* actor : targets)
		{
			RemoveActor(actor, /*cascadeToChildren=*/true);
		}
	}

	Actor* ResolveActor(ActorHandle handle) const { return m_actorPool.Resolve(handle); }	// Resolve an actor handle to an actor pointer

	ActorPool& GetActorPool() { return m_actorPool; }	// Get actor pool

	// Get root actors (actors without parents, owned by the scene)
	std::vector<Actor*> GetRootActors() const
	{
		std::vector<Actor*> result;
		m_actorPool.ForEach([&](Actor* actor) {
				if (actor->GetParentHandle().IsNull())
				{
					result.push_back(actor);
				}
			});
		return result;
	}

	// Get all actors in the scene (including children)
	std::vector<Actor*> GetAllActors() const
	{
		std::vector<Actor*> result;
		m_actorPool.ForEach([&](Actor* actor) { result.push_back(actor); });
		return result;
	}

	// Setters
	void SetDirectionalLight(const DirectionalLight& light) { m_directionalLight = light; }	// Set directional light
	void SetSceneManager(SceneManager* sceneManager) { m_pSceneManager = sceneManager; }	// Set scene manager	

	// Getters
	RenderSystem* GetRenderSystem() const { return m_pRenderSystem.get(); }				// Get render system
	CameraSystem* GetCameraSystem() const { return m_pCameraSystem.get(); }				// Get camera system
	CollisionSystem* GetCollisionSystem() const { return m_pCollisionSystem.get(); }	// Get collision system
	DirectionalLight GetDirectionalLight() const { return m_directionalLight; }			// Get directional light
	SceneManager* GetSceneManager() const { return m_pSceneManager; }					// Get scene manager
	EngineContext* GetEngineContext() const { return m_pEngineContext; }				// Get engine context

private:
	ActorPool m_actorPool;	// Actor pool (used for allocating actors)

	std::unique_ptr<RenderSystem> m_pRenderSystem = nullptr;		// Render system
	std::unique_ptr<CameraSystem> m_pCameraSystem = nullptr;		// Camera system
	std::unique_ptr<CollisionSystem> m_pCollisionSystem = nullptr;	// Collision system

	SceneManager* m_pSceneManager = nullptr;	// Pointer to the scene manager (used for scene switching)

	EngineContext* m_pEngineContext = nullptr;	// Pointer to the engine context (used for accessing engine systems)

protected:
	DirectionalLight m_directionalLight;	// Directional light
};
