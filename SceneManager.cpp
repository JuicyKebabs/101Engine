#include "SceneManager.h"
#include "Renderer.h"
#include "InputManager.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "EventManager.h"
#include "TitleScene.h"
#include "GameScene.h"

// Constructor
SceneManager::SceneManager(float windowWidth, float windowHeight)
{
	// Create scene elements and add to the list
	m_sceneElements.push_back(
		{
			SCENE_TYPE::SCENE_TITLE,
			std::make_unique<TitleScene>(windowWidth, windowHeight)
		}
	);
	m_sceneElements.push_back(
		{
			SCENE_TYPE::SCENE_GAME,
			std::make_unique<GameScene>(windowWidth, windowHeight)
		}
	);

	// Initialize scene class pointers
	m_currentScene = SCENE_TYPE::SCENE_GAME;
	m_pCurrentScene = GetSceneBase(m_currentScene);
}

// Destructor
SceneManager::~SceneManager()
{
}

// Initialize
void SceneManager::Initialize(EngineContext& context)
{
	m_context = context;

	// Scene change event registration
	using args = SCENE_TYPE;
	EventManager::GetInstance()->Subscribe<args>(
		EventType::CHANGE_SCENE,
		[this](std::shared_ptr<args> data)
		{
			ReserveChangeScene(*data);
		}
	);

	// Initialize current scene
	m_pCurrentScene->Initialize(context);
}

// Update
void SceneManager::Update(float deltaTime)
{
	m_pCurrentScene->Update(deltaTime);	// Update current scene

	// If there is a scene change reservation, change the scene
	if (m_sceneChangeReserved)
	{
		ChangeScene(m_reservedScene);
	}
}

// Finalization
void SceneManager::Finalize()
{
}

// Scene change reservation
void SceneManager::ReserveChangeScene(SCENE_TYPE newScene)
{
	m_sceneChangeReserved = true;	// Set scene change reservation flag
	m_reservedScene = newScene;		// Save reserved scene
}

// Change scene
void SceneManager::ChangeScene(SCENE_TYPE next)
{
	// Finalize current scene
	m_pCurrentScene->Finalize();

	// Switch to the next scene
	m_sceneChangeReserved = false;					// Reset scene change reservation flag
	m_reservedScene = SCENE_TYPE::SCENE_NONE;		// Reset reserved scene
	m_currentScene = next;							// Update current scene
	m_pCurrentScene = GetSceneBase(m_currentScene);	// Get scene class pointer

	// Scene change initialization
	m_pCurrentScene->Initialize(m_context);
}

// Submit draw requests
void SceneManager::SubmitDraws()
{
	m_pCurrentScene->Draw(m_context);
}

// Get camera information
CameraInfo* SceneManager::GetCameraInfo()
{
	return m_pCurrentScene->GetCameraInfo();
}

// Get scene class pointer by scene type
SceneBase* SceneManager::GetSceneBase(SCENE_TYPE sceneType)
{
	for (const auto& element : m_sceneElements)
	{
		if (element.sceneType == sceneType)
		{
			return element.pSceneBase.get();
		}
	}
}
