#pragma once
#include <windows.h>
#include "Engine/Engine.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Component/Camera.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Window/Window.h"
#include "Engine/Project/ProjectSettings.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Core/Debug/Debug.h"

// Application class
class App
{
public:
	static constexpr int WINDOW_WIDTH = 1920;	// Window width
	static constexpr int WINDOW_HEIGHT = 1080;	// Window height

private:
	Window m_window;

	App(const App&) = delete;				// Copy constructor disabled
	void operator=(const App&) = delete;	// Assignment operator disabled

private:
	HMODULE m_hGameCodeDll = nullptr;	// Game code DLL handle

	std::unique_ptr<Engine> m_pEngine = nullptr;					// DirectX12 engine pointer
	std::unique_ptr<Renderer> m_pRenderer = nullptr;				// Renderer pointer
	std::unique_ptr<SceneManager> m_pSceneManager = nullptr;		// Scene manager pointer
	std::unique_ptr<AssetManager> m_pAssetManager = nullptr;		// Asset Manager pointer
	std::unique_ptr<ActorImprintSystem> m_pActorImprintSystem;
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

	void InitSceneManager()
	{
		ProjectSettings settings;
		std::string settingsError;
		if (!ProjectSettings::Load(PathManager::Resolve("project.101"), settings, &settingsError))
		{
			DBG("App: Project settings could not be loaded: %s", settingsError.c_str());
			return;
		}
		for (const std::string& tag : settings.GetUserTags())
		{
			if (TagRegistry::Get().RegisterUserTag(tag, nullptr, &settingsError)) continue;
			DBG("App: Project Tag could not be registered: %s", settingsError.c_str());
			return;
		}
		if (settings.GetGameStartupSceneGuid().IsValid())
		{
			m_pSceneManager->SetInitialScene(settings.GetGameStartupSceneGuid());
		}
		m_pSceneManager->SetViewportSize(
			m_pEngine->GetFrameBufferWidth(),
			m_pEngine->GetFrameBufferHeight()
		);

		m_pSceneManager->Initialize(m_engineContext);	// Initialize scene manager with engine context
	}

	SceneManager* GetSceneManager() const { return m_pSceneManager.get(); }	// Get scene manager pointer

private:
	App() = default;	// Constructor

	bool ApplyWindowResizeRequest();

	void LoadGameCode();	// Load game code DLL

	void PrepareInstance();								// Prepare instance

	bool InitInstance();	// Initialize instance
	void Update();			// Update
	void Render();			// Draw
};
