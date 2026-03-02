#include <windows.h>
#include <algorithm>
#include <mmsystem.h>
#include "App.h"
#include <tchar.h>
#include "Keyboard.h"
#include "EventManager.h"

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
				Draw();		// Draw

				// Input update
				m_pInputManager->Copy(); // Copy key information from input manager

			}
		}
	} while (msg.message != WM_QUIT);	// Continue until quit message is received
}

// Termination
void App::Terminate()
{
	m_pEngine->Terminate();			// DirectX12 engine termination
	m_pAudioManager->Terminate();	// Audio manager termination

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
	wc.lpszClassName = _T("101_engine");					// Set class name
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
	// Get singleton instances of various classes
	m_pEngine = Engine::GetInstance();					// Get DirectX12 engine singleton instance
	m_pRenderer = Renderer::GetInstance();				// Get renderer singleton instance
	m_pSceneManager = SceneManager::GetInstance(		// Create scene management class
		static_cast<float>(WINDOW_WIDTH),
		static_cast<float>(WINDOW_HEIGHT)
	);
	m_pTextureManager = TextureManager::GetInstance();	// Create texture management class
	m_pMeshManager = MeshManager::GetInstance();		// Get mesh management class singleton instance
	m_pEventManager = EventManager::GetInstance();		// Get event management class singleton instance
	m_pAudioManager = AudioManager::GetInstance();		// Get audio management class singleton instance
	m_pInputManager = InputManager::GetInstance();		// Get input management class singleton instance
	m_pTime = Time::GetInstance();						// Get time management class singleton instance

	// Set up engine context structure
	m_engineContext = {
		Renderer::GetInstance(),
		TextureManager::GetInstance(),
		MeshManager::GetInstance()
	};
}

// Initialize instance
void App::InitInstance()
{
	// Initialize DirectX12 engine
	m_pEngine->Initialize(
		hwnd,			// Window handle
		WINDOW_WIDTH,	// Framebuffer width
		WINDOW_HEIGHT	// Framebuffer height
	);

	// Get device
	auto pDevice = m_pEngine->GetDevice();

	// Initialize texture management class
	m_pTextureManager->Initialize(
		pDevice,	// Device
		512			// Max descriptor count
	);

	// Initialize mesh management class
	m_pMeshManager->Initialize(
		pDevice	// Device
	);

	// Initialize renderer
	m_pRenderer->Initialize(
		pDevice,							// Device
		m_pSceneManager->GetCameraInfo(),	// Camera info structure
		m_pTextureManager					// Texture manager
	);

	// Initialize rendering
	m_pEngine->RenderBegin();

	// Initialize scene management class
	m_pSceneManager->Initialize(m_engineContext);

	// Initialize rendering
	m_pEngine->RenderEnd();

	// Initialize audio management class
	m_pAudioManager->Initialize();

	// Initialize input management class
	m_pInputManager->Initialize();

}

// Update
void App::Update()
{
	// Get current back buffer index
	const UINT backIdx = m_pEngine->GetCurrentBufferIndex();

	// Update various systems
	m_pRenderer->BeginFrame(backIdx);					// Frame start (clear internal queue)
	m_pInputManager->Update();							// Update input management class
	m_pSceneManager->Update(m_pTime->GetDeltaTime());	// Update scene management class
	m_pRenderer->Update(								// Update renderer
		backIdx, 
		*m_pSceneManager->GetCameraInfo()
	);
	m_pAudioManager->Update();			// Update audio management class
	m_pTime->Update();					// Update time management class
}

// Draw
void App::Draw()
{
	// Start rendering
	m_pEngine->RenderBegin();

	// Upload pending textures
	m_pTextureManager->UploadPendingTextures(m_pEngine->GetCommandList());

	// Submit draw requests for the game scene
	m_pSceneManager->SubmitDraws();

	// Draw the scene
	m_pRenderer->Draw(
		m_pEngine->GetCurrentBufferIndex(),	// Buffer index
		m_pEngine->GetCommandList()			// Command list
	);

	// End rendering
	m_pEngine->RenderEnd();
}
