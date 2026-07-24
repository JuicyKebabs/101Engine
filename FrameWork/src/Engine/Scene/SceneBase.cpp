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
	const auto collectedHandle =  m_actorPool.CollectGarbage();

	// Remove collected actors from the guid map
	for (const ActorHandle& handle : collectedHandle)
	{
		auto it = m_actorHandleGuidMap.find(handle);
		if (it != m_actorHandleGuidMap.end())
		{
			m_actorGuidMap.erase(it->second);
			m_actorHandleGuidMap.erase(it);
		}
	}
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

Actor* SceneBase::AddRootActor(std::unique_ptr<Actor> actor)
{
	return RegisterActor(std::move(actor), ActorHandle::Null());
}

Actor* SceneBase::AddChildActor(std::unique_ptr<Actor> actor, ActorHandle parentHandle)
{
	// Parent handle must be valid for a child actor
	if (parentHandle.IsNull()) return nullptr;

	return RegisterActor(std::move(actor), parentHandle);
}

Actor* SceneBase::RegisterActor(
	std::unique_ptr<Actor> actor,
	ActorHandle parentHandle
)
{
	if (!actor) return nullptr;

	Guid guid = actor->GetGuid();

	// Check if the actor's Guid is valid
	if (!guid.IsValid())
	{
		DBG("SceneBase::RegisterActor: Actor has an invalid Guid.");
		return nullptr;
	}

	// Check if the actor's Guid is already registered
	if (m_actorGuidMap.find(guid) != m_actorGuidMap.end())
	{
		DBG("SceneBase::RegisterActor: Duplicate Actor Guid: %s", guid.ToString().c_str());
		return nullptr;
	}

	// Check if the parent handle is valid (if not null)
	if (!parentHandle.IsNull() && !m_actorPool.IsValid(parentHandle))
	{
		DBG("SceneBase::RegisterActor: Parent handle is invalid.");
		return nullptr;
	}

	// Check if the parent actor is pending destruction
	if (!parentHandle.IsNull())
	{
		Actor* parent = m_actorPool.Resolve(parentHandle);
		if (!parent || parent->IsDestroyed())
		{
			DBG("SceneBase::RegisterActor: Parent is pending destruction.");
			return nullptr;
		}
	}

	// Set this scene as the owner of the actor
	actor->SetOwner(this);

	// Register the actor in the ActorPool
	const ActorHandle handle = m_actorPool.Register(std::move(actor));
	if (handle.IsNull())
	{
		DBG("SceneBase::RegisterActor: Failed to register actor in ActorPool.");
		return nullptr;
	}

	// Register handle and guid in the maps
	m_actorGuidMap.emplace(guid, handle);
	m_actorHandleGuidMap.emplace(handle, guid);

	// Resolve the actor pointer from the handle
	Actor* registered = m_actorPool.Resolve(handle);

	// Set the parent handle if provided
	if (registered && !parentHandle.IsNull())
	{
		registered->SetParentHandle(parentHandle);
	}

	return registered;
}
