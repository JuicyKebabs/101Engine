#pragma once
#include <windows.h>
#include <algorithm>
#include <mmsystem.h>
#include <tchar.h>
#include "EditorApp.h"
#include "Engine/Input/keyboard.h"
#include "Engine/Window/WindowInfo.h"
#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"

#pragma comment(lib, "winmm.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;

LRESULT EditorWindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_ACTIVATEAPP:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
		Keyboard_ProcessMessage(msg, wParam, lParam);
		break;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
			SendMessage(hwnd, WM_CLOSE, 0, 0);
		Keyboard_ProcessMessage(msg, wParam, lParam);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool EditorApp::Initialize()
{
	CreateMainWindow();	// Create main window
	PrepareInstance();	// Prepare instance
	InitInstance();		// Initialize instance
	InitImGui();		// Initialize ImGui
	return true;
}

void EditorApp::Run()
{
    ShowWindow(m_hwnd, SW_SHOW);

    MSG msg = {};
    DWORD dwExecLastTime;
    DWORD dwFPSLastTime;
    DWORD dwCurrentTime;
    DWORD dwFrameCount;

    timeBeginPeriod(1);
    dwExecLastTime = dwFPSLastTime = timeGetTime();
    dwCurrentTime = dwFrameCount = 0;

    do
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            dwCurrentTime = timeGetTime();

            if ((dwCurrentTime - dwFPSLastTime) >= 1000)
            {
                dwFPSLastTime = dwCurrentTime;
                dwFrameCount = 0;
            }

            if ((dwCurrentTime - dwExecLastTime) >= ((float)1000 / 60))
            {
                dwExecLastTime = dwCurrentTime;
                dwFrameCount++;

                float deltaTime = m_timeManager.GetDeltaTime();
                m_timeManager.Update();

                Update(deltaTime);
                Render();

                m_inputManager.Copy();
            }
        }
    } while (msg.message != WM_QUIT);
}

void EditorApp::Terminate()
{
    ShutdownImGui();
    m_pEngine->Terminate();
    UnregisterClass(m_wc.lpszClassName, m_wc.hInstance);
}

void EditorApp::CreateMainWindow()
{
    HINSTANCE hInst = GetModuleHandle(nullptr);

    m_wc.cbSize = sizeof(WNDCLASSEX);
    m_wc.style = CS_HREDRAW | CS_VREDRAW;
    m_wc.lpfnWndProc = (WNDPROC)EditorWindowProcedure;
    m_wc.hIcon = LoadIcon(hInst, IDI_APPLICATION);
    m_wc.hCursor = LoadCursor(hInst, IDC_ARROW);
    m_wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
    m_wc.lpszMenuName = nullptr;
    m_wc.lpszClassName = _T("101_Editor");
    m_wc.hInstance = hInst;

    RegisterClassEx(&m_wc);

    RECT wrc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    m_hwnd = CreateWindowEx(
        0,
        m_wc.lpszClassName,
        _T("101Editor"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wrc.right - wrc.left,
        wrc.bottom - wrc.top,
        NULL, NULL, hInst, NULL
    );
}

void EditorApp::PrepareInstance()
{
    WindowInfo::GetInstance().SetWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);

    m_pEngine = std::make_unique<Engine>();
    m_pRenderer = std::make_unique<Renderer>();
    m_pTextureManager = std::make_unique<TextureManager>();
    m_pMeshManager = std::make_unique<MeshManager>();

    m_engineContext = {
        m_pRenderer.get(),
        m_pTextureManager.get(),
        m_pMeshManager.get()
    };
}

void EditorApp::InitInstance()
{
    m_pEngine->InitCore(m_hwnd, WINDOW_WIDTH, WINDOW_HEIGHT);

    auto pDevice = m_pEngine->GetDevice();

    m_pTextureManager->Initialize(
        pDevice,
        m_pEngine->GetDescriptorHeapAllocator()
    );
    m_pMeshManager->Initialize(pDevice);
    m_pEngine->InitBindings(m_pTextureManager.get());
    m_pRenderer->Initialize(
        pDevice,
        m_pEngine->GetDescriptorHeapAllocator(),
        m_pTextureManager.get()
    );

    m_pEngine->BeginFrame();
    m_pEngine->RenderEnd();

    m_inputManager.Initialize();
}

void EditorApp::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(m_hwnd);

	auto descriptorHeapAllocator = m_pEngine->GetDescriptorHeapAllocator();
	uint32_t imguiIndex = descriptorHeapAllocator->AllocateCbvSrvUav();

    ImGui_ImplDX12_Init(
        m_pEngine->GetDevice(),
        Engine::FRAME_BUFFER_COUNT,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        descriptorHeapAllocator->GetCbvSrvUavHeap().GetHeap(),
		descriptorHeapAllocator->GetCbvSrvUavCpuHandle(imguiIndex),
		descriptorHeapAllocator->GetCbvSrvUavGpuHandle(imguiIndex)
	);

    io.Fonts->Build();
}

void EditorApp::Update(float deltaTime)
{
    m_inputManager.Update();
    m_pRenderer->Update(
        m_pEngine->GetCurrentBufferIndex(),
        CameraInfo{
            Vector3(0.0f, 0.0f, -5.0f),	// position
            Vector3(0.0f, 0.0f, 1.0f),		// forward
            Vector3(0.0f, 1.0f, 0.0f),		// up
            Matrix4x4::CreateLookAt(
                Vector3(0.0f, 0.0f, -5.0f),
                Vector3(0.0f, 0.0f, 0.0f),
                Vector3(0.0f, 1.0f, 0.0f)
            ),								// viewMatrix
            Matrix4x4::CreatePerspectiveFov(
                DegToRad(45.0f),
                static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT,
                0.1f,
                100.0f
            )								// projMatrix
		}
    );
}

void EditorApp::Render()
{
    m_pEngine->BeginFrame();
	m_pRenderer->BeginFrame(m_pEngine->GetCommandList());

	m_pTextureManager->UploadPendingTextures(m_pEngine->GetCommandList());

    RenderPassTarget backBufferTarget{
        RenderPassTargetType::BackBuffer,
        m_pEngine->GetCurrentBufferIndex()
    };
	m_pEngine->BeginPass(backBufferTarget);
    m_pRenderer->RenderScreenSpace(m_pEngine->GetCommandList());
    RenderImGui();
	m_pEngine->EndPass(backBufferTarget);

	m_pEngine->RenderEnd();
}

void EditorApp::RenderImGui()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

	ImGui::ShowDemoWindow();

    ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(
        ImGui::GetDrawData(), 
        m_pEngine->GetCommandList()
    );
}

void EditorApp::ShutdownImGui()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}