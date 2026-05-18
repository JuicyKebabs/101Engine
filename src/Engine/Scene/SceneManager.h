#pragma once
#include "SceneBase.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Scene/SceneTypes.h"

// Forward declaration
class Renderer;			// Renderer
class InputManager;		// Input manager
class TextureManager;	// Texture manager
class MeshManager;		// Mesh manager

// Scene element structure
struct SceneElement
{
	SCENE_TYPE sceneType = SCENE_TYPE::SCENE_NONE;	// Scene type
	std::unique_ptr<SceneBase> pSceneBase;			// Scene class pointer
};

// Scene management class
class SceneManager
{
public:
	SceneManager();		// Constructor
	~SceneManager();	// Destructor

	// Get singleton instance
	static SceneManager* GetInstance()
	{
		static SceneManager instance;
		return &instance;
	}

	// Main processing function
	void Initialize(EngineContext& context);
	void PreUpdate(float deltaTime);
	void Update(float deltaTime);
	void LateUpdate(float deltaTime);
	void OnRender();	// Draw request submission
	void Finalize();


	void ReserveChangeScene(SCENE_TYPE newScene);	// Scene change reservation
	void ChangeScene(SCENE_TYPE newScene);			// Scene change

	const CameraInfo* GetCameraInfo();	// Camera information retrieval

private:	// Member variables
	EngineContext m_context;					// Engine context
	std::vector<SceneElement> m_sceneElements;	// Scene element list

	SCENE_TYPE m_currentScene = SCENE_TYPE::SCENE_NONE;	// Current scene
	SceneBase* m_pCurrentScene = nullptr;				// Current scene class pointer

	bool m_sceneChangeReserved = false;						// Scene change reservation flag
	SCENE_TYPE m_reservedScene = SCENE_TYPE::SCENE_NONE;	// Reserved scene

private:
	SceneBase* GetSceneBase(SCENE_TYPE sceneType); // Get scene class pointer by scene type
};