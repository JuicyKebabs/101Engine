#pragma once
#include <windows.h>
#include "Engine/Engine.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Component/Camera.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/Input/InputManager.h"
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
	Engine* m_pEngine = nullptr;					// DirectX12 engine pointer
	Renderer* m_pRenderer = nullptr;				// Renderer pointer
	SceneManager* m_pSceneManager = nullptr;		// Scene manager pointer
	InputManager* m_pInputManager = nullptr;		// Input manager pointer
	TextureManager* m_pTextureManager = nullptr;	// Texture manager pointer
	MeshManager* m_pMeshManager = nullptr;			// Mesh manager pointer
	AudioManager* m_pAudioManager = nullptr;		// Audio manager pointer
	Time* m_pTime = nullptr;						// Time manager pointer

	EngineContext m_engineContext{};	// Engine context structure

public:
	App() {};	// Constructor
	~App(){};	// Destructor

	static App* GetInstance();	// Get singleton instance

	bool Initialize();	// Initialization
	void Run();			// Execution
	void Terminate();	// Termination

private:
	void CreateMainWindow(HWND& hwnd, WNDCLASSEX& wc);	// Create main window
	void PrepareInstance();								// Prepare instance

	void InitInstance();	// Initialize instance
	void Update();			// Update
	void Render();			// Draw
};