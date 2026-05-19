#include "SceneManager.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Core/Debug/Debug.h"

// Constructor
SceneManager::SceneManager()
{
	m_pCurrentScene = nullptr;
}

// Destructor
SceneManager::~SceneManager()
{
}

void SceneManager::RegisterScene(const std::string& name, std::unique_ptr<SceneBase> scene)
{
	m_sceneElements.push_back({ name, std::move(scene) });
}

void SceneManager::SetInitialScene(const std::string& name)
{
	m_currentSceneName = name;
}

// Initialize
void SceneManager::Initialize(EngineContext& context)
{
	m_context = context;
	m_pCurrentScene = GetSceneBase(m_currentSceneName);	// Get current scene class pointer
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
		ChangeScene(m_reservedSceneName);
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
void SceneManager::ReserveChangeScene(const std::string& newScene)
{
	m_sceneChangeReserved = true;	// Set scene change reservation flag
	m_reservedSceneName = newScene;		// Save reserved scene
}

// Submit draw requests
void SceneManager::OnRender()
{
	m_pCurrentScene->OnRender(m_context);
}

// Change scene
void SceneManager::ChangeScene(const std::string& next)
{
	// Finalize current scene
	m_pCurrentScene->Finalize();

	// Switch to the next scene
	m_sceneChangeReserved = false;						// Reset scene change reservation flag
	m_reservedSceneName.clear();						// Reset reserved scene
	m_currentSceneName = next;							// Update current scene
	m_pCurrentScene = GetSceneBase(m_currentSceneName);	// Get scene class pointer

	// Scene change initialization
	if (m_pCurrentScene)
	{
		m_pCurrentScene->Initialize(m_context);
	}
	else
	{
		DBG("Error: Scene '%s' not found. Unable to change scene.", m_currentSceneName.c_str());
	}
}

SceneBase* SceneManager::GetSceneBase(const std::string& name)
{
	for(auto& element : m_sceneElements)
	{
		if (element.name == name)
		{
			return element.pSceneBase.get();
		}
	}
	return nullptr;
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