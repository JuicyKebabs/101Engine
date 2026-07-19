#include "SceneBase.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Actor/ActorFactory.h"

// Constructor
SceneBase::SceneBase()
{
	m_pCameraSystem = std::make_unique<CameraSystem>();
	m_pRenderSystem = std::make_unique<RenderSystem>();
	m_pCollisionSystem = std::make_unique<CollisionSystem>();
}

// Destructor
SceneBase::~SceneBase()
{
}

// Initialization
void SceneBase::Initialize(EngineContext& context)
{
	m_pEngineContext = &context;
}

// Post-update (for late update)
void SceneBase::PreUpdate(float deltaTime)
{
	m_actorPool.ForEach([deltaTime](Actor* actor) {
		if (!actor->IsActive() || actor->IsDestroyed()) return;
		actor->PreUpdate(deltaTime);
		});
}

// Update
void SceneBase::Update(float deltaTime)
{
	m_actorPool.ForEach([deltaTime](Actor* actor) {
		if (!actor->IsActive() || actor->IsDestroyed()) return;
		actor->Update(deltaTime);
		});

	// Flush transforms recursively from root actors
	m_actorPool.ForEach([this](Actor* actor) {
		if (!actor->IsActive() || actor->IsDestroyed()) return;
		if (!actor->GetParentHandle().IsNull()) return;	// only recurse from roots to maintain correct order
		actor->FlushTransform();
		});
}

// Late update
void SceneBase::LateUpdate(float deltaTime)
{
	m_actorPool.ForEach([deltaTime](Actor* actor) {
		if (!actor->IsActive() || actor->IsDestroyed()) return;
		actor->LateUpdate(deltaTime);
		});

	// Flush transforms/collider
	m_actorPool.ForEach([this](Actor* actor) {
		if (!actor->IsActive() || actor->IsDestroyed()) return;
		if (!actor->GetParentHandle().IsNull()) return;
		actor->FlushTransform();
		actor->FlushColliderTransforms();
		});

	// Check colliders
	m_pCollisionSystem->CheckColliders();

	// Update collision system
	m_pCollisionSystem->CheckCollisions();

	//Flush camera parameters to update camera information
	if (m_pCameraSystem) {
		m_pCameraSystem->Flush(deltaTime);
	}

	// ActorPool owns the final OnDestroy + release step. All hierarchy-aware
	// destruction requests have already been marked through RemoveActor().
	m_actorPool.CollectGarbage();
}

// Render
void SceneBase::OnRender(
	EngineContext& context
)
{
	const auto* pCameraInfo = m_pCameraSystem->GetCameraInfo();

	// Check if main camera exists before rendering.
	if (!pCameraInfo)
	{
		//DBG("SceneBase::OnRender: No main camera set, skipping render.");		
		return;
	}

	auto cameraInfo = *pCameraInfo;	// Make a copy of the camera info to pass to the render system (in case the render system needs to modify it for sorting or other purposes)

	m_pRenderSystem->FlushRegisters();
	m_pRenderSystem->BuildFrameRenderData(cameraInfo);
	context.pRenderer->SubmitFrameRenderData(m_pRenderSystem->GetFrameRenderData());
	context.pRenderer->SubmitCameraInfo(cameraInfo);
	context.pRenderer->SubmitDirectionalLight(m_directionalLight);
}

// Finalization
void SceneBase::Finalize()
{
	m_pCollisionSystem->ClearColliders();
}
