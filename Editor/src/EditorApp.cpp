#include <windows.h>
#include <algorithm>
#include <mmsystem.h>
#include <tchar.h>
#include "EditorApp.h"
#include "Engine/Input/keyboard.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Window/WindowInfo.h"
#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"
#include "EditorScene.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/Camera.h"
#include "Engine/Scene/SceneLoader.h"
#include "Engine/Scene/SceneWriter.h"
#include "Engine/Core/Debug/Debug.h"
#include "BehaviorTemplateGenerator.h"
#include "ProjectBuilder.h"
#include "Engine/Scene/ComponentRegistry.h"

#pragma comment(lib, "winmm.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static const int WINDOW_WIDTH  = 1280;
static const int WINDOW_HEIGHT = 720;

static const char* kDefaultScenePath = "asset/scenes/test.scene";

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
	// Load the game code DLL at startup. This is needed to recognize GameCode-defined
    m_hGameCodeDll = LoadLibraryA("GameCode.dll");
    if (!m_hGameCodeDll)
    {
        DBG("EditorApp: Failed to load GameCode.dll (error %lu)", GetLastError());
    }
    else
    {
        DBG("EditorApp: GameCode.dll loaded successfully");
    }

    CreateMainWindow();    // Create main window
    PrepareInstance();     // Prepare instance
    InitInstance();        // Initialize instance
    InitImGui();           // Initialize ImGui

    NewScene();            // Start with a fresh scene (MainCamera-tagged DefaultCamera)

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

                // ImGui's NewFrame is called before Update so that debug
                // ImGui windows (e.g. EditorCamera's "Camera Info") can be
                // drawn from within Update().
                ImGui_ImplDX12_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                Update(deltaTime);
                Render();

                InputManager::GetInstance().Copy();
            }
        }
    } while (msg.message != WM_QUIT);
}

void EditorApp::Terminate()
{
	// Destroy the scene while the game code DLL is still loaded
    m_hierarchyPanel.ClearSelection();
    m_pScene.reset();
    ComponentRegistry::Get().UnregisterAllGameComponents();
    if (m_hGameCodeDll)
    {
        FreeLibrary(m_hGameCodeDll);
        m_hGameCodeDll = nullptr;
    }

	// Terminate editor related resources
    ShutdownImGui();
    m_pEngine->Terminate();
    UnregisterClass(m_wc.lpszClassName, m_wc.hInstance);
}

// Create a new scene with default settings
// (a single DefaultCamera actor tagged "MainCamera")
void EditorApp::NewScene()
{
    m_pScene = std::make_unique<EditorScene>();
    m_pScene->Initialize(m_engineContext);

    Actor::InitDesc cameraDesc;
    cameraDesc.name = "DefaultCamera";
    cameraDesc.tag = ActorTags::MainCamera;

    auto* cameraActor = ActorFactory::CreateActor(ActorType::Camera, cameraDesc);
    cameraActor->GetComponentByClass<Transform>()->SetParams(
        Transform::ParamDesc(Vector3{ 0, 0, -5 })
    );
    auto* camera = cameraActor->GetComponentByClass<Camera>();
    camera->SetParams(Camera::ParamDesc{
        .window_width = WINDOW_WIDTH,
        .window_height = WINDOW_HEIGHT
        });
    m_pScene->AddRootActor(cameraActor);
    m_pScene->GetCameraSystem()->SetMainCamera(camera);

    DBG("EditorApp: New scene created.");
}

// Load a scene from a file path
void EditorApp::LoadScene(const std::string& filePath)
{
    m_pScene = std::make_unique<EditorScene>();
    m_pScene->Initialize(m_engineContext);

    bool result = SceneLoader::LoadScene(filePath, m_pScene.get(), m_engineContext);

    if (result) DBG("EditorApp: Loaded scene from %s", filePath.c_str());
    else        DBG("EditorApp: Failed to load scene from %s", filePath.c_str());
}

// Hot reload: rebuild GameCode.dll and reload it without restarting the Editor.
//
// Order of operations matters for safety:
//   1. Save the current scene (so we can reconstruct it afterwards)
//   2. Destroy the scene while the OLD GameCode.dll is still loaded,
//      so any GameCode-owned component destructors are still valid.
//   3. Remove GameCode-side factories from ComponentRegistry. Their
//      std::function objects point into the old DLL's code, which is
//      about to disappear.
//   4. FreeLibrary the old DLL. This also releases the file lock on
//      GameCode.dll so the build below can overwrite it.
//   5. Rebuild GameCode.dll.
//   6. LoadLibrary the new DLL. Its static initializers run here,
//      re-registering all REGISTER_GAME_COMPONENT components.
//   7. Reconstruct the scene from the saved snapshot. SceneLoader's
//      AddToActor calls now resolve through the new DLL's factories.
//
// If the build or load fails, the scene is still restored from the
// snapshot (just without GameCode components), so nothing is lost -
// fix the code and try again.
void EditorApp::ReloadGameCode()
{
	// 1. Save the current scene to a temporary file
	static const char* kHotReloadScenePath = "asset/scenes/_hotreload_temp.scene";   // file path for hot-reload snapshot

    if (!m_pScene)
    {// In case of empty scene
        DBG("EditorApp: ReloadGameCode - no active scene, aborting.");
        return;
    }

    if (!SceneWriter::SaveScene(kHotReloadScenePath, m_pScene.get()))
    {// In case of save failure
        DBG("EditorApp: ReloadGameCode - failed to save scene snapshot, aborting reload.");
        return;
    }

	// 2. Destroy the current scene while the old DLL is still loaded
	m_hierarchyPanel.ClearSelection();   // Clear selection to avoid dangling pointers to a selected actor
	m_pScene->Finalize();
	m_pScene.reset();

	// 3. Unregister GameCode-side factories from ComponentRegistry
	ComponentRegistry::Get().UnregisterAllGameComponents();

    // 4. Free the old DLL
    if (m_hGameCodeDll)
    {
        FreeLibrary(m_hGameCodeDll);
        m_hGameCodeDll = nullptr;
        DBG("EditorApp: Unloaded old GameCode.dll");
	}

    // 5. Rebuild GameCode.dll
    if (ProjectBuilder::Build("GameCode", "Debug"))
    {
		// 6. Load the new DLL
		m_hGameCodeDll = LoadLibraryA("GameCode.dll");
        if (m_hGameCodeDll)
        {
            DBG("EditorApp: ReloadGameCode - GameCode.dll reloaded successfully.");
        }
        else
        {
            DBG("EditorApp: ReloadGameCode - LoadLibrary failed (error %lu)", GetLastError());
        }
	}
    else
    {
        DBG("EditorApp: ReloadGameCode - build failed. Scene will be restored without GameCode components.");
    }

	// 7. Reconstruct the scene from the saved snapshot
	LoadScene(kHotReloadScenePath);
}

void EditorApp::CreateMainWindow()
{
    HINSTANCE hInst = GetModuleHandle(nullptr);

    m_wc.cbSize        = sizeof(WNDCLASSEX);
    m_wc.style         = CS_HREDRAW | CS_VREDRAW;
    m_wc.lpfnWndProc   = (WNDPROC)EditorWindowProcedure;
    m_wc.hIcon         = LoadIcon(hInst, IDI_APPLICATION);
    m_wc.hCursor       = LoadCursor(hInst, IDC_ARROW);
    m_wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
    m_wc.lpszMenuName  = nullptr;
    m_wc.lpszClassName = _T("101_Editor");
    m_wc.hInstance     = hInst;

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

    m_pEngine         = std::make_unique<Engine>();
    m_pRenderer       = std::make_unique<Renderer>();
    m_pTextureManager = std::make_unique<TextureManager>();
    m_pMeshManager    = std::make_unique<MeshManager>();

    m_engineContext = {
        m_pRenderer.get(),
        m_pTextureManager.get(),
        m_pMeshManager.get()
    };

    // Editor-only free-fly camera actor (not part of any SceneBase)
    Actor::InitDesc camDesc;
    camDesc.name = "EditorCamera";
    m_pEditorCameraActor.reset(ActorFactory::CreateEmptyActor(camDesc));
    m_pEditorCamera = m_pEditorCameraActor->AddComponent<EditorCamera>();
    m_pEditorCamera->Initialize(WINDOW_WIDTH, WINDOW_HEIGHT);
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

    InputManager::GetInstance().Initialize();
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
    InputManager::GetInstance().Update();

    // Update the editor camera actor
    // (call PreUpdate, Update, and LateUpdate in sequence to update camera component)
    m_pEditorCameraActor->PreUpdate(deltaTime);
    m_pEditorCameraActor->Update(deltaTime);
    m_pEditorCameraActor->LateUpdate(deltaTime);

    // Flush the transform of the editor camera actor
    m_pEditorCameraActor->FlushTransform();

    // Update the scene
    // (call PreUpdate, Update, and LateUpdate in sequence to update all actors and components in the scene)
    if (m_pScene)
    {
        m_pScene->PreUpdate(deltaTime);
        m_pScene->Update(deltaTime);
        m_pScene->LateUpdate(deltaTime);
    }

    // Update the renderer with the latest camera information (for rendering this frame)
    m_pRenderer->Update(m_pEngine->GetCurrentBufferIndex(), m_pEditorCamera->GetCameraInfo());
}

void EditorApp::Render()
{
    m_pEngine->BeginFrame();
    m_pRenderer->BeginFrame(m_pEngine->GetCommandList());

    m_pTextureManager->UploadPendingTextures(m_pEngine->GetCommandList());

    if (m_pScene) m_pScene->OnRender(m_engineContext);

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
    MenuBar::Callbacks callbacks;

    callbacks.onNewScene = [this]()
    {
        NewScene();
    };

    callbacks.onOpenScene = [this]()
    {
        LoadScene(kDefaultScenePath);
    };

    callbacks.onSaveScene = [this]()
    {
        if (m_pScene)
        {
            if (!SceneWriter::SaveScene(kDefaultScenePath, m_pScene.get()))
            {
                DBG("EditorApp: Save failed.");
            }
        }
    };

    callbacks.onBuildGame = []()
    {
        ProjectBuilder::ReconfigureAndBuild("101Game", "Debug");
    };

    callbacks.onReloadGameCode = [this]()
    {
        ReloadGameCode();
    };

    callbacks.onCreateBehavior = [](const std::string& name)
    {
        if (BehaviorTemplateGenerator::Generate(name))
        {
            DBG("EditorApp: Generated behavior template '%s'", name.c_str());

            ProjectBuilder::ReconfigureAndBuild("101Game", "Debug");
        }
    };

    m_menuBar.Render(callbacks);

    m_hierarchyPanel.Render(m_pScene.get());
    m_inspectorPanel.Render(m_hierarchyPanel.GetSelectedActor());

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
