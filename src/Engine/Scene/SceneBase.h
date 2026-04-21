#pragma once
#include "Engine/Component/Camera.h"
#include "Engine/Core/ComPtr/ComPtr.h"
#include "Engine/Core/Utility/SharedStruct.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Actor/Actor.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Physics/CollisionManager.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Graphics/RenderSystem.h"
#include "Engine/Graphics/CameraSystem.h"

// Scene base class
// Base class for all scene classes
class SceneBase
{
public:
	static constexpr DirectX::XMFLOAT3 SKY_BOX_SIZE = { 50.0f, 50.0f, 50.0f }; // Skybox size
public:
	SceneBase(float window_width, float window_height);	// Constructor
	~SceneBase();										// Destructor

	// Main processing functions
	void Initialize(EngineContext& context);	// Initialization
	void PreUpdate(float deltaTime);			// Pre-update
	void Update(float deltaTime);				// Update
	void LateUpdate(float deltaTime);			// Late update
	void OnRender(EngineContext& context);		// Render
	void Finalize();							// Finalize
	void ResolveCollisions() {};				// Resolve collisions

	// Add an actor
	template<class T, class... Args>
	T* AddActor(Args&&... args) {
		static_assert(std::is_base_of_v<Actor, T>, "AddActor<T>: T must derive from Actor");
		auto actor = std::make_unique<T>(std::forward<Args>(args)...);
		T* ptr = actor.get();
		actor->SetOwner(this);
		m_addPendingActors.push_back(std::move(actor));
		return ptr;
	}
	void AddActorToPending(std::unique_ptr<Actor> actor) {
		actor->SetOwner(this);
		m_addPendingActors.push_back(std::move(actor));
	}

	RenderSystem* GetRenderSystem() const { return m_pRenderSystem.get(); }	// Get render system
	CameraSystem* GetCameraSystem() const { return m_pCameraSystem.get(); }	// Get camera system

private:
	std::vector<std::unique_ptr<Actor>> m_actors;		// Object list in the scene
	std::vector<std::unique_ptr<Canvas>> m_canvasList;	// Canvas list in the scene

	std::vector<std::unique_ptr<Actor>> m_addPendingActors;	// Pending objects to be added

	std::unique_ptr<RenderSystem> m_pRenderSystem = nullptr;	// Render system
	std::unique_ptr<CameraSystem> m_pCameraSystem = nullptr;	// Camera system

	//std::unique_ptr<CollisionManager> m_pCollisionManager = nullptr;	// Collision manager

protected:
	DirectionalLight m_directionalLight;	// Directional light

private:
	virtual void InitializeOverride(EngineContext& context) = 0;	// Initialization (override in derived class)
};