#pragma once
#include <windows.h>
#include "Engine/Engine.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Component/Camera.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Core/Context/Context.h"

// Application class
class App
{
public:
	static constexpr int WINDOW_WIDTH = 1920;	// Window width
	static constexpr int WINDOW_HEIGHT = 1080;	// Window height

private:
	HINSTANCE hInstance = nullptr;	// Instance handle
	HWND hwnd = nullptr;			// Window handle

	WNDCLASSEX wc = {};	// Window class

	App(const App&) = delete;				// Copy constructor disabled
	void operator=(const App&) = delete;	// Assignment operator disabled

private:
	HMODULE m_hGameCodeDll = nullptr;	// Game code DLL handle

	std::unique_ptr<Engine> m_pEngine = nullptr;					// DirectX12 engine pointer
	std::unique_ptr<Renderer> m_pRenderer = nullptr;				// Renderer pointer
	std::unique_ptr<SceneManager> m_pSceneManager = nullptr;		// Scene manager pointer
	AssetManager& m_assetManager = AssetManager::GetInstance();		// Asset Manager reference
	std::unique_ptr<TextureManager> m_pTextureManager = nullptr;	// Texture manager pointer
	std::unique_ptr<MeshManager> m_pMeshManager = nullptr;			// Mesh manager pointer

	TimeManager& m_time = TimeManager::GetInstance();			// Time manager reference
	InputManager& m_inputManager = InputManager::GetInstance();	// Input manager reference
	AudioManager& m_audioManager = AudioManager::GetInstance();	// Audio manager reference

	EngineContext m_engineContext{};	// Engine context structure

public:
	~App()= default;	// Destructor

	static App* GetInstance();	// Get singleton instance

	bool Initialize();	// Initialization
	void Run();			// Execution
	void Terminate();	// Termination

	void InitSceneManager(){
		m_pSceneManager->Initialize(m_engineContext);	// Initialize scene manager with engine context
	}

	SceneManager* GetSceneManager() const { return m_pSceneManager.get(); }	// Get scene manager pointer

private:
	App() = default;	// Constructor

	void LoadGameCode();	// Load game code DLL

	void CreateMainWindow(HWND& hwnd, WNDCLASSEX& wc);	// Create main window
	void PrepareInstance();								// Prepare instance

	void InitInstance();	// Initialize instance
	void Update();			// Update
	void Render();			// Draw
};