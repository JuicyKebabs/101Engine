#pragma once
#include <windows.h>
#include "Engine.h"
#include "Renderer.h"
#include "Camera.h"
#include "SceneManager.h"
#include "InputManager.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "EventManager.h"
#include "Audio.h"
#include "Time.h"
#include "Context.h"

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
	EventManager* m_pEventManager = nullptr;		// Event manager pointer
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
	void Draw();			// Draw
};