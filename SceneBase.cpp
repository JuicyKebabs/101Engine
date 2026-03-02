#include "SceneBase.h"
#include "Renderer.h"
#include "InputManager.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "EventManager.h"
#include "CollisionManager.h"

// Constructor
SceneBase::SceneBase(float window_width, float window_height)
{
	m_pCamera = new CameraComponent(nullptr, window_width, window_height);	// Generate camera component
	m_pCollisionManager = new CollisionManager();			// Generate collision manager
	m_pEffectManager = new EffectManager();					// Generate effect manager
}

// Destructor
SceneBase::~SceneBase()
{
	delete m_pCamera;
	delete m_pCollisionManager;
	delete m_pEffectManager;
	m_actorList.clear();
}

// Initialization
void SceneBase::Initialize(EngineContext& context)
{
	// Camera initialization
	m_pCamera->Initialize();

	// Collision manager initialization
	m_pCollisionManager->Initialize(context);

	// Effect manager initialization
	m_pEffectManager->Initialize(
		*context.pTextureManager,
		*context.pMeshManager
	);

	// Scene specific initialization call
	InitializeOverride(context);
}

// Update
void SceneBase::Update(float deltaTime)
{
#ifdef _DEBUG
	// Toggle collider drawing with P key
	m_drawColliders |= InputManager::GetInstance()->GetInputInfo()->key.p.trigger;
#endif // _DEBUG
	

	// Scene specific update call
	UpdateOverride();

	// Update every object in scene
	for (auto& actor : m_actorList)
	{
		actor->Update(deltaTime);
	}

	// Update scene specific
	//m_pCamera->Update();

	// Check colliders
	m_pCollisionManager->CheckColliders();

	// Update collision manager
	m_pCollisionManager->CheckCollisions();

	// Scene specific collision resolution
	ResolveCollisions();

	// Resolve collisions for each object
	for(auto& object : m_actorList)
	{
		object->LateUpdate();
	}

	// Update effect manager
	m_pEffectManager->Update();
}

// Draw
void SceneBase::Draw(
	EngineContext& context
)
{
}

// Finalization
void SceneBase::Finalize()
{
	// Scene specific finalization call
	FinalizeOverride();
	m_pCollisionManager->ClearColliders();
	m_pEffectManager->Finalize();

	// Unsubscribe events
	for (const auto& eventData : m_eventDataList)
	{
		EventManager::GetInstance()->Unsubscribe(eventData.type, eventData.id);
	}
	m_eventDataList.clear();
}

//ÉJÉÅÉâèÓïÒéÊìæ
CameraInfo* SceneBase::GetCameraInfo() const
{
	return &m_pCamera->GetCameraInfo();
}

//Å@Add object to scene
void SceneBase::AddObject(std::unique_ptr<Actor> object)
{
	m_actorList.push_back(std::move(object));
}
