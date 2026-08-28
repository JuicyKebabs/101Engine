#include <windows.h>
#include <algorithm>
#include <mmsystem.h>
#include "App.h"
#include <tchar.h>
#include "Engine/Input/keyboard.h"
#include "Engine/EngineComponentrRegistration.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Scene/ComponentRegistry.h"

#pragma comment(lib, "winmm.lib")

// Get singleton instance
App* App::GetInstance()
{
	static App instance; // Singleton instance
	return &instance;
}

// Initialization
bool App::Initialize()
{
	LoadGameCode();	// Load game code DLL

	Window::InitDesc windowDesc{};
	windowDesc.className = L"101EngineGameWindow";
	windowDesc.title = L"101Engine";
	windowDesc.clientWidth = WINDOW_WIDTH;
	windowDesc.clientHeight = WINDOW_HEIGHT;
	windowDesc.allowFullscreenToggle = true;
	windowDesc.messageCallback = [](
		HWND hwnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam,
		LRESULT& outResult)
		{
			switch (message)
			{
			case WM_ACTIVATEAPP:
			case WM_SYSKEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYUP:
				Keyboard_ProcessMessage(message, wParam, lParam);
				break;

			case WM_KEYDOWN:
				if (wParam == VK_ESCAPE)
				{
					SendMessage(hwnd, WM_CLOSE, 0, 0);
				}
				Keyboard_ProcessMessage(message, wParam, lParam);
				break;
			}

			return false;
		};

	if (!m_window.Initialize(windowDesc))
	{
		DBG("App: Failed to initialize the main window.");
		return false;
	}

	PrepareInstance(); // Prepare instance

	InitInstance(); // Initialize instance

	return true;
}

// Execution
void App::Run()
{
	// Show window
	m_window.Show();

	// Message loop
	MSG msg = {};	// Message

	// Frame rate measurement variables
	DWORD	dwExecLastTime;	// Last execution time
	DWORD	dwFPSLastTime;	// Last FPS measurement time
	DWORD	dwCurrentTime;	// Current time
	DWORD	dwFrameCount;	// Frame count

#ifdef _DEBUG	// Debug build only FPS display
	int		countFPS = {};		// FPS counter
	wchar_t debugStr[64]{};		// FPS display string
#endif

	// Frame rate measurement initialization
	timeBeginPeriod(1);								// Set timer resolution
	dwExecLastTime = dwFPSLastTime = timeGetTime();	// Get current timer value
	dwCurrentTime = dwFrameCount = 0;				// Initialize

	do 
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{// Message is available
			TranslateMessage(&msg);	// Translate message
			DispatchMessage(&msg);	// Dispatch message
		}
		else
		{// Message is not available
			dwCurrentTime = timeGetTime();	// Get current timer value

			if ((dwCurrentTime - dwFPSLastTime) >= 1000)
			{
#ifdef _DEBUG
				countFPS = dwFrameCount;	// FPS count save
#endif
				dwFPSLastTime = dwCurrentTime;	// Save current timer value
				dwFrameCount = 0;				// Initialize frame count
			}

			if ((dwCurrentTime - dwExecLastTime) >= ((float)1000 / 60))
			{// Time to execute next frame
				dwExecLastTime = dwCurrentTime;	// Save current timer value
				dwFrameCount++;					// Increment frame count
#ifdef _DEBUG
				// Update window title with FPS
				swprintf_s(debugStr, L"101Engine FPS : %d", countFPS);
				SetWindowTextW(m_window.GetHandle(), debugStr);
#endif
				if (!m_window.ApplyPendingModeChange())
				{
					DBG("App: Failed to apply the pending window mode change.");
				}

				if (m_window.IsMinimized())
				{
					m_time.Update();
					continue;
				}

				if (!ApplyWindowResizeRequest())
				{
					continue;
				}

				// Update
				Update();	// Update

				// Draw
				Render();		// Draw

				// Input update
				m_inputManager.Copy(); // Copy key information from input manager

			}
		}
	} while (msg.message != WM_QUIT);	// Continue until quit message is received
}

// Termination
void App::Terminate()
{
	m_pSceneManager->Finalize();
	ComponentRegistry::Get().UnregisterAllGameComponents();

	if (m_hGameCodeDll)
	{
		FreeLibrary(m_hGameCodeDll);
		m_hGameCodeDll = nullptr;
	}

	m_pEngine->Terminate();		// DirectX12 engine termination
	m_audioManager.Terminate();	// Audio manager termination

	m_window.Terminate();
}

bool App::ApplyWindowResizeRequest()
{
	Window::Size requestedSize{};

	// Check if there is a pending resize request
	if (!m_window.GetResizeRequest(requestedSize))
	{
		return true; // No resize request pending
	}

	// Resize the engine output to match the requested window size
	if (!m_pEngine->ResizeOutput(requestedSize.width, requestedSize.height))
	{
		DBG("App: Failed to resize engine output to %ux%u", requestedSize.width, requestedSize.height);
		return false;
	}

	// Resize the scene render targets to match the requested window size
	if (!m_pEngine->ResizeSceneRenderTargets(requestedSize.width, requestedSize.height))
	{
		return false;
	}

	// Update the scene manager's viewport size to match the requested window size
	m_pSceneManager->SetViewportSize(requestedSize.width, requestedSize.height);

	m_window.CommitResize();

	return true;
}

// Load game code DLL
void App::LoadGameCode()
{
	m_hGameCodeDll = LoadLibrary(_T("GameCode.dll"));	// Load GameCode.dll

	if (!m_hGameCodeDll)
	{
		DBG("Failed to load GameCode.dll");
	}
	else
	{
		DBG("Successfully loaded GameCode.dll");
	}
}

void App::PrepareInstance()
{
	// Get singleton instances of various classes
	m_pEngine = std::make_unique<Engine>();
	m_pRenderer = std::make_unique<Renderer>();
	m_pSceneManager = std::make_unique<SceneManager>();
	m_pTextureManager = std::make_unique<TextureManager>();
	m_pMeshManager = std::make_unique<MeshManager>();	
	m_pAssetManager = std::make_unique<AssetManager>();

	// Set up engine context structure
	m_engineContext = {
		m_pRenderer.get(),
		m_pTextureManager.get(),
		m_pMeshManager.get(),
		m_pAssetManager.get()
	};
}

// Initialize instance
void App::InitInstance()
{
	// Initialize DirectX12 engine
	m_pEngine->InitCore(
		m_window.GetHandle(),	// Window handle
		WINDOW_WIDTH,	// Framebuffer width
		WINDOW_HEIGHT	// Framebuffer height
	);

	// Get device
	auto pDevice = m_pEngine->GetDevice();

	// Initialize texture management class
	m_pTextureManager->Initialize(
		pDevice,								// Device
		m_pEngine->GetDescriptorHeapAllocator()	// Descriptor heap allocator
	);

	// Initialize mesh management class
	m_pMeshManager->Initialize(
		pDevice,				// Device
		m_pTextureManager.get()	// Texture manager
	);

	// Initialize asset manager
	m_pAssetManager->Initialize(
		PathManager::Resolve("asset"),
		m_pTextureManager.get(),
		m_pMeshManager.get()
		);

	// Initialize engine bindings
	m_pEngine->InitBindings(m_pTextureManager.get());

	// Initialize renderer
	m_pRenderer->Initialize(
		pDevice,									// Device
		m_pEngine->GetDescriptorHeapAllocator(),	// Descriptor heap allocator
		m_pTextureManager.get(),					// Texture manager
		m_pMeshManager.get()						// Mesh manager
	);

	// Initialize rendering
	m_pEngine->BeginFrame();

	// Initialize rendering
	m_pEngine->EndFrame();

	// Initialize audio management class
	m_audioManager.Initialize();

	// Initialize input management class
	m_inputManager.Initialize();
}

// Update
void App::Update()
{
	// Update time manager and get delta time
	m_time.Update();
	float deltaTime = m_time.GetDeltaTime();

	// Update various systems
	m_inputManager.Update();				// Update input management class
	m_audioManager.Update();				// Update audio management class
	m_pSceneManager->PreUpdate(deltaTime);	// Pre-update scene management class (for late update)
	m_pSceneManager->Update(deltaTime);		// Update scene management class
	m_pSceneManager->LateUpdate(deltaTime);	// Post-update scene management class (for late update)
	m_pRenderer->Update(					// Update renderer
		m_pEngine->GetCurrentBufferIndex(),
		*m_pSceneManager->GetCameraInfo()
	);
}

// Draw
void App::Render()
{
	// Start rendering
	m_pEngine->BeginFrame();
	m_pRenderer->BeginFrame(m_pEngine->GetCommandList());

	// Upload pending textures
	m_pTextureManager->UploadPendingTextures(m_pEngine->GetCommandList());

	// Submit draw requests for the game scene
	m_pSceneManager->OnRender();

	// Render the scene depth into the shadow map
	RenderPassTarget shadowTarget
	{
		RenderPassTargetType::DepthOnly,
		RenderPassTarget::InvalidIndex,
		static_cast<uint32_t>(Engine::BuiltinRenderTarget::ShadowMap)
	};	
	m_pEngine->BeginPass(shadowTarget);
	m_pRenderer->RenderShadowMap(m_pEngine->GetCommandList());
	m_pEngine->EndPass(shadowTarget);

	// Render the scene to the main render target
	RenderPassTarget sceneTarget
	{
		RenderPassTargetType::ColorDepth,
		static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneColor),
		static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneDepth)
	};
	m_pEngine->BeginPass(sceneTarget);
	uint32_t shadowMapSrvIndex = m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::ShadowMap)->GetSrvIndex();
	m_pRenderer->RenderScene(m_pEngine->GetCommandList(), shadowMapSrvIndex);
	m_pEngine->EndPass(sceneTarget);

	// Draw for back buffer
	RenderPassTarget backBufferTarget
	{
		RenderPassTargetType::BackBuffer,
		m_pEngine->GetCurrentBufferIndex(),
		RenderPassTarget::InvalidIndex
	};	
	m_pEngine->BeginPass(backBufferTarget);
	m_pRenderer->RenderFullScreenPass(m_pEngine->GetCommandList(), m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::SceneColor));
	m_pRenderer->RenderScreenSpace(
		m_pEngine->GetCommandList(),
		m_pEngine->GetFrameBufferWidth(),
		m_pEngine->GetFrameBufferHeight(),
		RenderTargetFormat::LDR
		);
	m_pEngine->EndPass(backBufferTarget);

	// End rendering
	m_pEngine->EndFrame();
}
