#include "SceneBase.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Actor/ActorFactory.h"

// Constructor
SceneBase::SceneBase()
{
	m_pCameraSystem = std::make_unique<CameraSystem>();
	m_pRenderSystem = std::make_unique<RenderSystem>();
	m_pCollisionSystem = std::make_unique<CollisionSystem>();

	// Create default camera actor and set it as the main camera actor
	Actor::InitDesc cameraParamDesc;
	cameraParamDesc.name = "DefaultCamera";
	auto defaultCameraActor = AddRootActor(ActorFactory::CreateActor(ActorType::Camera, cameraParamDesc));
	defaultCameraActor->GetComponentByClass<Transform>()->SetParams(Transform::ParamDesc(Vector3{ 0,0,-5 }));
	auto camera = defaultCameraActor->GetComponentByClass<Camera>();
	camera->SetParams(Camera::ParamDesc{.window_width = WindowInfo::GetInstance().GetWidth(), .window_height = WindowInfo::GetInstance().GetHeight()});
	m_pCameraSystem->SetMainCamera(camera);
}

// Destructor
SceneBase::~SceneBase()
{
}

// Initialization
void SceneBase::Initialize(EngineContext& context)
{
	InitializeOverride(context);
}

// Post-update (for late update)
void SceneBase::PreUpdate(float deltaTime)
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

	// Pre update every actor in list
	for (auto& actor : m_rootActors)
	{
		if (!actor->IsActive() || actor->IsDestroyed()) continue;
		actor->PreUpdate(deltaTime);
	}
}

// Update
void SceneBase::Update(float deltaTime)
{	
	// Update every object in scene
	for (auto& actor : m_rootActors)
	{
		if (!actor->IsActive() || actor->IsDestroyed()) continue;
		actor->Update(deltaTime);
	}

	// Flush transforms of all actors to update world transforms before collision checks
	for(auto& actor : m_rootActors)
	{
		if (!actor->IsActive() || actor->IsDestroyed()) continue;
		actor->FlushTransform();
	}
}

// Late update
void SceneBase::LateUpdate(float deltaTime)
{
	// Late update every actor in scene
	for (auto& actor : m_rootActors)
	{
		if (!actor->IsActive() || actor->IsDestroyed()) continue;
		actor->LateUpdate(deltaTime);
	}

	// Flush transforms of all actors again
	for(auto& actor : m_rootActors)
	{
		if (!actor->IsActive() || actor->IsDestroyed()) continue;
		actor->FlushTransform();
		actor->FlushColliderTransforms();
	}

	// Check colliders
	m_pCollisionSystem->CheckColliders();

	// Update collision system
	m_pCollisionSystem->CheckCollisions();

	//Flush camera parameters to update camera information
	if (m_pCameraSystem) {
		m_pCameraSystem->Flush(deltaTime);
	}

	// Destroy actors from scene( from both root actors list and all actors list)
	m_rootActors.erase(std::remove_if(m_rootActors.begin(), m_rootActors.end(),
		[](const std::unique_ptr<Actor>& actor) { return actor->IsDestroyed(); }), m_rootActors.end());
	m_allActors.erase(std::remove_if(m_allActors.begin(), m_allActors.end(),
		[](Actor* actor) { return actor->IsDestroyed(); }), m_allActors.end());
}

// Render
void SceneBase::OnRender(
	EngineContext& context
)
{
	auto cameraInfo = *m_pCameraSystem->GetCameraInfo();
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