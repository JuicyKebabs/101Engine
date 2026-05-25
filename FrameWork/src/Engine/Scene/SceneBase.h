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

	// Add an actor
	Actor* AddActor(Actor* actor) {
		if (!actor) return nullptr;
		actor->SetOwner(this);
		m_actors.push_back(std::unique_ptr<Actor>(actor));
		return actor;
	}

	void AddActorToPending(std::unique_ptr<Actor> actor) {
		actor->SetOwner(this);
		m_addPendingActors.push_back(std::move(actor));
	}

	RenderSystem* GetRenderSystem() const { return m_pRenderSystem.get(); }				// Get render system
	CameraSystem* GetCameraSystem() const { return m_pCameraSystem.get(); }				// Get camera system
	CollisionSystem* GetCollisionSystem() const { return m_pCollisionSystem.get(); }	// Get collision system

	void SetDirectionalLight(const DirectionalLight& light) { m_directionalLight = light; }	// Set directional light

private:
	std::vector<std::unique_ptr<Actor>> m_actors;			// Object list in the scene
	std::vector<std::unique_ptr<Actor>> m_addPendingActors;	// Pending objects to be added

	std::unique_ptr<RenderSystem> m_pRenderSystem = nullptr;		// Render system
	std::unique_ptr<CameraSystem> m_pCameraSystem = nullptr;		// Camera system
	std::unique_ptr<CollisionSystem> m_pCollisionSystem = nullptr;	// Collision system

protected:
	DirectionalLight m_directionalLight;	// Directional light

private:
	virtual void InitializeOverride(EngineContext& context) = 0;	// Initialization (override in derived class)
};