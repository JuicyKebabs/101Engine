#include "SceneManager.h"
#include "SceneLoader.h"
#include "Engine/Core/Path/PathManager.h"
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

// Register a scene created in code
void SceneManager::RegisterScene(const std::string& name, std::unique_ptr<SceneBase> scene)
{
	m_sceneElements.push_back({ name, std::move(scene) });
}

// Register a scene from a JSON file
void SceneManager::RegisterSceneFile(const std::string& name, const std::string& jsonPath)
{
	// Scene pointer is not created in this function
	// Scene will be created when the scene is changed, using the JSON file

	SceneElement elem;
	elem.name = name;
	elem.jsonPath = jsonPath;
	m_sceneElements.push_back(std::move(elem));
	DBG("SceneManager: Registered scene file '%s' -> '%s'", name.c_str(), jsonPath.c_str());
}

void SceneManager::SetInitialScene(const std::string& name)
{
	m_currentSceneName = name;
}

// Initialize
void SceneManager::Initialize(EngineContext& context)
{
	m_context = context;

	// Note : 
	// The name of the initial scene must be set by the parameter of json file which define whole game data.
	// But such kind of json file is not implemented yet, 
	// so the initial scene name is set by SetInitialScene() function for now.

	ChangeScene(m_currentSceneName);	// Change to the initial scene
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
	m_reservedSceneName = newScene;	// Save reserved scene
}

// Submit draw requests
void SceneManager::OnRender()
{
	m_pCurrentScene->OnRender(m_context);
}

// Change scene
void SceneManager::ChangeScene(const std::string& next)
{
	// Get the next scene element
	SceneElement* pNextElem = GetSceneElement(next);
	if (!pNextElem)
	{
		DBG("SceneManager: Scene '%s' not found", next.c_str());
		return;
	}

	// Finalize current scene
	if (m_pCurrentScene)
	{
		m_pCurrentScene->Finalize();	// Finalize current scene
		m_pCurrentScene = nullptr;		// Reset current scene pointer
	}

	// Switch to the next scene
	m_sceneChangeReserved = false;	// Reset scene change reservation flag
	m_reservedSceneName.clear();	// Reset reserved scene
	m_currentSceneName = next;		// Update current scene

	// Switch to the next scene
	if(!pNextElem->jsonPath.empty())
	{// If the next scene is registerd with json file, load the scene from the json file

		// Initialize new scene
		pNextElem->pSceneBase = std::make_unique<SceneBase>();
		pNextElem->pSceneBase->Initialize(m_context);

		// Load scene from JSON file
		std::string fullPath = PathManager::Resolve(pNextElem->jsonPath);
		if (!SceneLoader::LoadScene(fullPath, pNextElem->pSceneBase.get(), m_context))
		{
			DBG("SceneManager: Failed to load scene from '%s'", fullPath.c_str());
		}
	}
	else
	{// If the next scene is registerd with code, use the scene pointer directly
		m_pCurrentScene = pNextElem->pSceneBase.get();
		if (!m_pCurrentScene)
		{
			DBG("SceneManager: Scene '%s' is not initialized.", next.c_str());
			return;
		}
	}

	// Set the current scene pointer
	m_pCurrentScene = pNextElem->pSceneBase.get();

	// Set the scene manager for the current scene
	if (m_pCurrentScene)
	{
		m_pCurrentScene->SetSceneManager(this);
	}

	m_currentSceneName = next;
	DBG("SceneManager: Changed to scene '%s'", next.c_str());
}

SceneElement* SceneManager::GetSceneElement(const std::string& name)
{
	for(auto& element : m_sceneElements)
	{
		if (element.name == name)
		{
			return &element;
		}
	}
	DBG("Error: Scene element '%s' not found.", name.c_str());

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