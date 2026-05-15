#include "SceneManager.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Core/Debug/Debug.h"
#include "Game/TitleScene.h"
#include "Game/GameScene.h"

// Constructor
SceneManager::SceneManager()
{
	// Create scene elements and add to the list
	m_sceneElements.push_back(
		{
			SCENE_TYPE::SCENE_TITLE,
			std::make_unique<TitleScene>()
		}
	);
	m_sceneElements.push_back(
		{
			SCENE_TYPE::SCENE_GAME,
			std::make_unique<GameScene>()
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

	// Initialize current scene
	m_pCurrentScene->Initialize(context);
}

// Pre-update
void SceneManager::PreUpdate(float deltaTime)
{
	m_pCurrentScene->PreUpdate(deltaTime);
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

// Late update
void SceneManager::LateUpdate(float deltaTime)
{
	m_pCurrentScene->LateUpdate(deltaTime);
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

// Submit draw requests
void SceneManager::OnRender()
{
	m_pCurrentScene->OnRender(m_context);
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

// Get camera information
const CameraInfo* SceneManager::GetCameraInfo()
{
	auto cameraSystem = m_pCurrentScene->GetCameraSystem();
	if (!cameraSystem) {
		DBG("Error: Current scene does not have a camera system. Unable to retrieve camera information.");
		return nullptr;
	}

	return cameraSystem->GetCameraInfo();
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

	return nullptr;
}