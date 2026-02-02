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
	m_pCamera = new Camera(window_width, window_height);	// Generate camera
	m_pCollisionManager = new CollisionManager();			// Generate collision manager
	m_pEffectManager = new EffectManager();					// Generate effect manager
}

// Destructor
SceneBase::~SceneBase()
{
	delete m_pCamera;
	delete m_pCollisionManager;
	delete m_pEffectManager;
	m_objectList.clear();
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

	for(auto& object : m_objectList)
	{
		object->CreateRenderModel(
			*context.pTextureManager,
			*context.pMeshManager
		);
	}

	// Create render info for skybox
	CreateRenderInfo(
		*context.pTextureManager,
		*context.pMeshManager,
		&m_skyboxRenderInfo,
		MESH_TYPE::IMPORT,
		BLEND_MODE::BLEND_OPAQUE,
		L"asset/fbx/sky_box/sky_box.fbx",
		false
	);
}

// Update
void SceneBase::Update()
{
#ifdef _DEBUG
	// Toggle collider drawing with P key
	m_drawColliders |= InputManager::GetInstance()->GetInputInfo()->key.p.trigger;
#endif // _DEBUG
	

	// Scene specific update call
	UpdateOverride();

	// Update every object in scene
	for (auto& object : m_objectList)
	{
		object->Update();
	}

	// Update scene specific
	m_pCamera->Update();

	// Check colliders
	m_pCollisionManager->CheckColliders();

	// Update collision manager
	m_pCollisionManager->CheckCollisions();

	// Scene specific collision resolution
	ResolveCollisions();

	// Resolve collisions for each object
	for(auto& object : m_objectList)
	{
		object->ResolveCollisions();
	}

	// Update effect manager
	m_pEffectManager->Update();
}

// Draw
void SceneBase::Draw(
	EngineContext& context
)
{
	Renderer& pRenderer = *context.pRenderer;

	// Submit directional light
	pRenderer.SubmitDirectionalLight(m_directionalLight);

	// Submit skybox render info
	pRenderer.SubmitToWorldList(
			BuildRenderInfoForSubmit(
			m_skyboxRenderInfo, 
			MESH_TYPE::IMPORT, 
			m_pCamera->GetCameraInfo().position,
			SKY_BOX_SIZE
	));

	// Submit render info for each object
	for(auto& object : m_objectList)
	{
		object->SubmitDraws(pRenderer);
	}

	// Submit effect draws
	m_pEffectManager->SubmitDraws(pRenderer);

	// Submit canvas draws
	for (auto& canvas : m_canvasList)
	{
		canvas->SubmitDraws(pRenderer);
	}

#ifdef _DEBUG
	if (m_drawColliders)
	{
		// Draw colliders
		m_pCollisionManager->Draw(pRenderer);
	}
#endif // _DEBUG
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
	m_objectList.push_back(std::move(object));
}
