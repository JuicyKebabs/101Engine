#include "SceneBase.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Physics/CollisionManager.h"
#include "Engine/Core/Debug/Debug.h"
#include "Game/CameraTest.h"

// Constructor
SceneBase::SceneBase(float window_width, float window_height)
{
	m_pCameraSystem = std::make_unique<CameraSystem>();
	m_pRenderSystem = std::make_unique<RenderSystem>();
	//m_pCollisionManager = std::make_unique<CollisionManager>();

	// Create default camera actor and set it as the main camera actor
	auto defaultCameraActor = AddActor<Actor>();
	auto camera = defaultCameraActor->AddComponent<Camera>(window_width, window_height);
	m_pCameraSystem->SetMainCamera(camera);
}

// Destructor
SceneBase::~SceneBase()
{
	m_actors.clear();
}

// Initialization
void SceneBase::Initialize(EngineContext& context)
{
	// Collision manager initialization
	//m_pCollisionManager->Initialize(context);
	InitializeOverride(context);
}

// Post-update (for late update)
void SceneBase::PreUpdate(float deltaTime)
{
	// Add pending actors to the scene
	for (auto& pendingActor : m_addPendingActors)
	{
		m_actors.push_back(std::move(pendingActor));
	}
	m_addPendingActors.clear();

	// Pre update every actor in list
	for (auto& actor : m_actors)
	{
		if (!actor->IsActive() || actor->IsDestroyed()) continue;
		actor->PreUpdate(deltaTime);
	}
}

// Update
void SceneBase::Update(float deltaTime)
{	
	// Update every object in scene
	for (auto& actor : m_actors)
	{
		if (!actor->IsActive() || actor->IsDestroyed()) continue;
		actor->Update(deltaTime);
	}

	// Flush transforms of all actors to update world transforms before collision checks
	for(auto& actor : m_actors)
	{
		if (!actor->IsActive() || actor->IsDestroyed()) continue;
		actor->FlushTransform();
	}
}

// Late update
void SceneBase::LateUpdate(float deltaTime)
{
	// Late update every actor in scene
	for (auto& actor : m_actors)
	{
		if (!actor->IsActive() || actor->IsDestroyed()) continue;
		actor->LateUpdate(deltaTime);
	}

	// Check colliders
	//m_pCollisionManager->CheckColliders();

	// Update collision manager
	//m_pCollisionManager->CheckCollisions();

	// Scene specific collision resolution
	//ResolveCollisions();

	// Flush transforms of all actors again
	for(auto& actor : m_actors)
	{
		if (!actor->IsActive() || actor->IsDestroyed()) continue;
		actor->FlushTransform();
	}

	//Flush camera parameters to update camera information
	if (m_pCameraSystem) {
		m_pCameraSystem->Flush(deltaTime);
	}

	// Destroy actors from scene
	m_actors.erase(std::remove_if(m_actors.begin(), m_actors.end(),
		[](const std::unique_ptr<Actor>& actor) { return actor->IsDestroyed(); }),
		m_actors.end());

}

// Render
void SceneBase::OnRender(
	EngineContext& context
)
{
	auto cameraInfo = *m_pCameraSystem->GetCameraInfo();
	m_pRenderSystem->BuildDrawPackets(cameraInfo);
	context.pRenderer->SubmitDrawPacket(m_pRenderSystem->GetDrawPackets());
	context.pRenderer->SubmitCameraInfo(cameraInfo);
}

// Finalization
void SceneBase::Finalize()
{
	//m_pCollisionManager->ClearColliders();
}