#pragma once
#include "SceneBase.h"
#include "Engine/Core/Context/Context.h"

// Forward declaration
class Renderer;			// Renderer
class InputManager;		// Input manager
class TextureManager;	// Texture manager
class MeshManager;		// Mesh manager

// Scene element structure
struct SceneElement
{
	std::string name;						// Scene name (for debugging)
	std::unique_ptr<SceneBase> pSceneBase;	// Scene class pointer
	std::string jsonPath;					// JSON file path for scene loading
};

// Scene management class
class SceneManager
{
public:
	SceneManager();		// Constructor
	~SceneManager();	// Destructor

	// Main processing function
	void Initialize(EngineContext& context);
	void PreUpdate(float deltaTime);
	void Update(float deltaTime);
	void LateUpdate(float deltaTime);
	void OnRender();	// Draw request submission
	void Finalize();

	void RegisterScene(const std::string& name, std::unique_ptr<SceneBase> scene);	// Scene registration
	void RegisterSceneFile(const std::string& name, const std::string& jsonPath);	// Scene registration (from JSON file)
	
	void SetInitialScene(const std::string& name);									// Set initial scene (for testing)
	void ReserveChangeScene(const std::string& name);								// Scene change reservation

	const CameraInfo* GetCameraInfo();	// Camera information retrieval

private:
	EngineContext m_context;					// Engine context
	std::vector<SceneElement> m_sceneElements;	// Scene element list

	SceneBase* m_pCurrentScene = nullptr;	// Current scene class pointer
	std::string m_currentSceneName;			// Current scene name (for debugging)

	bool m_sceneChangeReserved = false;	// Scene change reservation flag
	std::string m_reservedSceneName;	// Reserved scene name for scene change

private:
	void ChangeScene(const std::string& name);				// Scene change
	SceneElement* GetSceneElement(const std::string& name);	// Get scene element by name
};