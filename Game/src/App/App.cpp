#include <windows.h>
#include <algorithm>
#include <mmsystem.h>
#include "App.h"
#include <tchar.h>
#include "Engine/Input/keyboard.h"
#include "Engine/Window/WindowInfo.h"
#include "Engine/EngineComponentrRegistration.h"

#pragma comment(lib, "winmm.lib")

// Window procedure
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ACTIVATEAPP:	// Active window has changed
	case WM_SYSKEYDOWN:		// System key pressed
	case WM_KEYUP:			// Key released
	case WM_SYSKEYUP:		// System key released
		// Keyboard message processing
		Keyboard_ProcessMessage(msg, wParam, lParam);
		break;

	case WM_KEYDOWN:		// Key pressed
		if (wParam == VK_ESCAPE)// Pressed key is ESC

		{
			// Send request to close the window
			SendMessage(hwnd, WM_CLOSE, 0, 0);
		}

		// Keyboard message processing
		Keyboard_ProcessMessage(msg, wParam, lParam);
		break;

	case WM_DESTROY:		// Window destroyed
		PostQuitMessage(0);	// Send quit message to OS
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Get singleton instance
App* App::GetInstance()
{
	static App instance; // Singleton instance
	return &instance;
}

// Initialization
bool App::Initialize()
{
	CreateMainWindow(hwnd, wc);	// Create main window

	PrepareInstance(); // Prepare instance

	InitInstance(); // Initialize instance

	return true;
}

// Execution
void App::Run()
{
	// Show window
	ShowWindow(hwnd, SW_SHOW);

	// Message loop
	MSG msg = {};	// Message

	// Frame rate measurement variables
	DWORD	dwExecLastTime;	// Last execution time
	DWORD	dwFPSLastTime;	// Last FPS measurement time
	DWORD	dwCurrentTime;	// Current time
	DWORD	dwFrameCount;	// Frame count

#ifdef _DEBUG	// Debug build only FPS display
	int		countFPS = {};		// FPS counter
	char	debugStr[2048];	// FPS display string
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
				wsprintf(debugStr, "DX21 プロジェクト ");	// Message string
				wsprintf(
					&debugStr[strlen(debugStr)],	// String concatenation
					" FPS : %d", countFPS			// FPS
				);
				SetWindowText(hwnd, debugStr);	// Set window title
#endif
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
	m_pEngine->Terminate();			// DirectX12 engine termination
	m_audioManager.Terminate();	// Audio manager termination

	UnregisterClass(wc.lpszClassName, wc.hInstance);	// Unregister window class
}

// Create main window
void App::CreateMainWindow(HWND& hwnd, WNDCLASSEX& wc)
{
	HINSTANCE hInst = GetModuleHandle(nullptr);
	
	// Set up window class
	wc.cbSize = sizeof(WNDCLASSEX);							// Size of structure
	wc.style = CS_HREDRAW | CS_VREDRAW;						// Redraw on horizontal/vertical resize
	wc.lpfnWndProc = (WNDPROC)WindowProcedure;				// Set callback function
	wc.hIcon = LoadIcon(hInstance, IDI_APPLICATION);		// Set icon
	wc.hCursor = LoadCursor(hInstance, IDC_ARROW);			// Set cursor
	wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);	// Set background color
	wc.lpszMenuName = nullptr;								// Set menu
	wc.lpszClassName = _T("101_Engine");					// Set class name
	wc.hInstance = GetModuleHandle(NULL);					// Set instance handle

	// Register window class
	RegisterClassEx(&wc);

	// Set up window rectangle
	RECT wrc = { 0,0, WINDOW_WIDTH, WINDOW_HEIGHT };

	// Calculate window size based on desired client area size
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// Create window object
	hwnd = CreateWindowEx(
		0,						// Extended window style
		wc.lpszClassName,		// Set class name
		_T("DX12_Application"),	// Set title bar text
		WS_OVERLAPPEDWINDOW,	// Set window style
		CW_USEDEFAULT,			// Set X position
		CW_USEDEFAULT,			// Set Y position
		wrc.right - wrc.left,	// Set window width
		wrc.bottom - wrc.top,	// Set window height
		NULL,					// Set parent window handle
		NULL,					// Set menu handle
		hInstance,				// Set instance handle
		NULL					// Set additional parameters
	);
}

void App::PrepareInstance()
{
	WindowInfo::GetInstance().SetWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);

	// Get singleton instances of various classes
	m_pEngine = std::make_unique<Engine>();
	m_pRenderer = std::make_unique<Renderer>();
	m_pSceneManager = std::make_unique<SceneManager>();
	m_pTextureManager = std::make_unique<TextureManager>();
	m_pMeshManager = std::make_unique<MeshManager>();	

	// Set up engine context structure
	m_engineContext = {
		m_pRenderer.get(),
		m_pTextureManager.get(),
		m_pMeshManager.get()
	};
}

// Initialize instance
void App::InitInstance()
{
	// Initialize DirectX12 engine
	m_pEngine->InitCore(
		hwnd,			// Window handle
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
		pDevice	// Device
	);

	// Initialize engine bindings
	m_pEngine->InitBindings(m_pTextureManager.get());

	// Initialize renderer
	m_pRenderer->Initialize(
		pDevice,									// Device
		m_pEngine->GetDescriptorHeapAllocator(),	// Descriptor heap allocator
		m_pTextureManager.get()						// Texture manager
	);

	// Initialize rendering
	m_pEngine->BeginFrame();

	// Initialize rendering
	m_pEngine->RenderEnd();

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

	// Render the shadow depth on map
	RenderPassTarget shadowTarget{ RenderPassTargetType::DepthOnly, static_cast<uint32_t>(Engine::BuiltinRenderTarget::ShadowMap) };
	m_pEngine->BeginPass(shadowTarget);
	m_pRenderer->RenderShadowMap(m_pEngine->GetCommandList());
	m_pEngine->EndPass(shadowTarget);

	// Render the scene to the main render target
	RenderPassTarget sceneTarget{ RenderPassTargetType::ColorDepth, static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneColor) };
	m_pEngine->BeginPass(sceneTarget);
	uint32_t shadowMapSrvIndex = m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::ShadowMap)->GetSrvIndex();
	m_pRenderer->RenderScene(m_pEngine->GetCommandList(), shadowMapSrvIndex);
	m_pEngine->EndPass(sceneTarget);

	// Draw for back buffer
	RenderPassTarget backBufferTarget{ RenderPassTargetType::BackBuffer, m_pEngine->GetCurrentBufferIndex() };
	m_pEngine->BeginPass(backBufferTarget);
	m_pRenderer->RenderFullScreenPass(m_pEngine->GetCommandList(), m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::SceneColor));
	m_pRenderer->RenderScreenSpace(m_pEngine->GetCommandList());
	m_pEngine->EndPass(backBufferTarget);

	// End rendering
	m_pEngine->RenderEnd();
}
