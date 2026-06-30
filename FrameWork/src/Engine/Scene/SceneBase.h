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

//----------------------------------------------------------------------------------------
// SceneBase class
// This class represents a base of scene
// Scene has a list of root actors and all actors in the scene
// This also has systems which are used to process the core functionalities of the scene
//----------------------------------------------------------------------------------------

class SceneManager;	// Forward declaration of SceneManager class

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

	// Remove an actor from the scene (mark it for destruction)
	void RemoveActor(Actor* actor)
	{
		if (!actor) return;
		actor->Destroy();
	}

	// Remove an actor from the scene by name
	void RemoveActor(const std::string& name)
	{
		// Dstroy the actor with the given name in the scene (if it exists)
		for(auto it = m_allActors.begin(); it != m_allActors.end(); ++it)
		{
			if ((*it)->GetName() == name)
			{
				(*it)->Destroy();
			}
		}
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

	// Get all actors in the scene (including children)
	std::vector<Actor*> GetAllActors() const { return m_allActors; }

	// Move all pending actors (root + flat list) into the active containers.
	// Must be called before iterating GetRootActors()/GetAllActors() outside of
	// the normal PreUpdate loop (e.g. SceneWriter::SaveScene, SceneLoader after load).
	void FlushPendingActors()
	{
		for (auto& pendingActor : m_addPendingRootActors)
		{
			m_rootActors.push_back(std::move(pendingActor));
		}
		m_addPendingRootActors.clear();

		for (auto* pendingActor : m_addPendingAllActors)
		{
			m_allActors.push_back(pendingActor);
		}
		m_addPendingAllActors.clear();
	}

	// Getters for systems
	RenderSystem* GetRenderSystem() const { return m_pRenderSystem.get(); }				// Get render system
	CameraSystem* GetCameraSystem() const { return m_pCameraSystem.get(); }				// Get camera system
	CollisionSystem* GetCollisionSystem() const { return m_pCollisionSystem.get(); }	// Get collision system

	// Setters and getters for directional light
	void SetDirectionalLight(const DirectionalLight& light) { m_directionalLight = light; }	// Set directional light
	DirectionalLight GetDirectionalLight() const { return m_directionalLight; }				// Get directional light

	void SetSceneManager(SceneManager* sceneManager) { m_pSceneManager = sceneManager; }	// Set scene manager	
	SceneManager* GetSceneManager() const { return m_pSceneManager; }						// Get scene manager

private:
	std::vector<std::unique_ptr<Actor>> m_rootActors;			// Root actors (actors without parents, owned by the scene)
	std::vector<Actor*> m_allActors;							// Raw pointer array of all actors in the scene (including children)
	std::vector<std::unique_ptr<Actor>> m_addPendingRootActors;	// Pending root actors to be added (owned by the scene)
	std::vector<Actor*> m_addPendingAllActors;					// Raw pointer array of pending objects to be added (including children)

	std::unique_ptr<RenderSystem> m_pRenderSystem = nullptr;		// Render system
	std::unique_ptr<CameraSystem> m_pCameraSystem = nullptr;		// Camera system
	std::unique_ptr<CollisionSystem> m_pCollisionSystem = nullptr;	// Collision system

	SceneManager* m_pSceneManager = nullptr;	// Pointer to the scene manager (used for scene switching)

protected:
	DirectionalLight m_directionalLight;	// Directional light
};
