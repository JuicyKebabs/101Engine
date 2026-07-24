#include <windows.h>
#include <algorithm>
#include <mmsystem.h>
#include <tchar.h>
#include <shellapi.h> 
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
#include "ClassTemplateGenerator.h"
#include "ProjectBuilder.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Core/Path/PathManager.h"
#include "Command/CreateActorCommand.h"

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

    // Clear the command history
    m_commandHistory.Clear();

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
    // Clear inspector info to avoid dangling pointers to the soon-to-be-destroyed scene's actors/components
    m_hierarchyPanel.ClearSelection();

	// Clear the command history
	m_commandHistory.Clear();

	// Create a new scene instance and initialize it
    m_pScene = std::make_unique<EditorScene>();
    m_pScene->Initialize(m_engineContext);

	// Create a default camera actor and set it as the main camera in the scene's camera system
    Actor::InitDesc cameraDesc;
    cameraDesc.name = "DefaultCamera";
    cameraDesc.tag = ActorTags::MainCamera;

    auto cameraActorOwned = ActorFactory::CreateActor(ActorType::Camera, cameraDesc);
    cameraActorOwned->GetComponentByClass<Transform>()->SetParams(
		Transform::ParamDesc{
			.localPosition = { 0, 0, -5 }
		}
    );
    auto* camera = cameraActorOwned->GetComponentByClass<Camera>();
    camera->SetParams(Camera::ParamDesc{
        .window_width = WINDOW_WIDTH,
        .window_height = WINDOW_HEIGHT
        });
    m_pScene->AddRootActor(std::move(cameraActorOwned));
    m_pScene->GetCameraSystem()->SetMainCamera(camera);

    DBG("EditorApp: New scene created.");
}

// Load a scene from a file path
void EditorApp::LoadScene(const std::string& filePath)
{
    // Clear inspector info to avoid dangling pointers to the soon-to-be-destroyed scene's actors/components
    m_hierarchyPanel.ClearSelection();

    // Clear the command history
    m_commandHistory.Clear();

	// Create a new scene instance and initialize it
    m_pScene = std::make_unique<EditorScene>();
    m_pScene->Initialize(m_engineContext);

	// Load the scene data from file
    bool result = SceneLoader::LoadScene(filePath, m_pScene.get());

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
void EditorApp::ReloadGameCode(bool reconfigure)
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
    m_commandHistory.Clear();            // Clear the command history
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
	// Don't rebuild dependencies (e.g. 101Framework)
    (void)reconfigure;
    bool buildSucceeded = ProjectBuilder::BuildGameCodeForHotReload("Debug");

    if (buildSucceeded)
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

void EditorApp::DeleteScript(const std::string& name)
{
	namespace fs = std::filesystem;

	// Construct the paths for the header and source files
	std::string baseDir = PathManager::Resolve("Game/GameCode/");
    std::string headerPath = baseDir + name + ".h";
    std::string sorthePath = baseDir + name + ".cpp";

	bool deletedAny = false;

	// Delete the header file if it exists
    if (fs::exists(headerPath))
    {
        fs::remove(headerPath);
        DBG("EditorApp: Deleted script header %s", headerPath.c_str());
        deletedAny = true;
    }

	// Delete the source file if it exists
	if (fs::exists(sorthePath))
	{
		fs::remove(sorthePath);
		DBG("EditorApp: Deleted script source %s", sorthePath.c_str());
		deletedAny = true;
	}

	// If neither file was found, log a message and return(Don't attempt to rebuild the project)
    if (!deletedAny)
    {
        DBG("EditorApp: No files found for script '%s'", name.c_str());
        return;
    }

    // Reconfigure and rebuild the project to reflect the deletion of the script files
	ReloadGameCode(true); 
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
	m_pAssetManager   = std::make_unique<AssetManager>();

    m_engineContext = {
        m_pRenderer.get(),
        m_pTextureManager.get(),
        m_pMeshManager.get()
    };

    // Editor-only free-fly camera actor (not part of any SceneBase)
    Actor::InitDesc camDesc;
    camDesc.name = "EditorCamera";
    m_pEditorCameraActor = ActorFactory::CreateEmptyActor(camDesc);
    m_pEditorCamera = m_pEditorCameraActor->AddComponent<EditorCamera>();
    m_pEditorCamera->Initialize(WINDOW_WIDTH, WINDOW_HEIGHT);
}

void EditorApp::InitInstance()
{
    m_pEngine->InitCore(m_hwnd, WINDOW_WIDTH, WINDOW_HEIGHT);

    auto pDevice = m_pEngine->GetDevice();

    m_pTextureManager->Initialize(pDevice, m_pEngine->GetDescriptorHeapAllocator());
    m_pMeshManager->Initialize(pDevice, m_pTextureManager.get());
	m_pAssetManager->Initialize(PathManager::GetProjectRoot(), m_pTextureManager.get(), m_pMeshManager.get());
    m_pEngine->InitBindings(m_pTextureManager.get());
    m_pRenderer->Initialize(pDevice, m_pEngine->GetDescriptorHeapAllocator(), m_pTextureManager.get(), m_pMeshManager.get());

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

    RenderPassTarget sceneTarget{ RenderPassTargetType::ColorDepth, static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneColor) };
    
	// Render the scene to the shadow map render target first
    m_pEngine->BeginPass(sceneTarget);
    uint32_t shadowMapSrvIndex = m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::ShadowMap)->GetSrvIndex();
    m_pRenderer->RenderScene(m_pEngine->GetCommandList(), shadowMapSrvIndex);
    m_pEngine->EndPass(sceneTarget);

	// Render the scene to the scene color render target
    RenderPassTarget backBufferTarget{ RenderPassTargetType::BackBuffer, m_pEngine->GetCurrentBufferIndex() };
    m_pEngine->BeginPass(backBufferTarget);
    RenderImGui();
    m_pEngine->EndPass(backBufferTarget);

    m_pEngine->RenderEnd();
}

void EditorApp::RenderImGui()
{
	// Render the MenuBar with its callbacks
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

        callbacks.onUndo = [this]()
            {
				// Avoid dangling pointers to a selected actor that may be deleted by the undo operation
                m_hierarchyPanel.ClearSelection();

                if (m_commandHistory.Undo())
                {
                    DBG("EditorApp: Undo succeeded.");
                }
                else
                {
                    DBG("EditorApp: Undo failed.");
                }
            };

        callbacks.onRedo = [this]()
            {
                m_hierarchyPanel.ClearSelection();

                if (m_commandHistory.Redo())
                {
                    DBG("EditorApp: Redo succeeded.");
                }
                else
                {
                    DBG("EditorApp: Redo failed.");
                }
            };

        callbacks.onBuildGame = []()
            {
                ProjectBuilder::ReconfigureAndBuild("101Game", "Debug");
            };

        callbacks.onReloadGameCode = [this](bool reconfigure)
            {
                ReloadGameCode(reconfigure);
            };

        callbacks.onCreateScript = [this](const std::string& name, bool isBehavior)
            {
                // Generate the new script files
                bool generated = isBehavior
                    ? BehaviorTemplateGenerator::Generate(name) // Behavior class
                    : ClassTemplateGenerator::Generate(name);   // Regular class

                if (generated)
                {
                    DBG("EditorApp: Generated %s template '%s'",
                        isBehavior ? "Behavior" : "class", name.c_str());

                    ReloadGameCode(true);   // Reconfigure and build to pick up the new behavior
                }
            };

        callbacks.canUndo = m_commandHistory.CanUndo();
        callbacks.canRedo = m_commandHistory.CanRedo();

        m_menuBar.Render(callbacks);
    }

	// Render the Scripts panel with its callbacks
    {
        ScriptsPanel::Callbacks scriptCallbacks;

        // Delete a script
        scriptCallbacks.onDelete = [this](const std::string& name)
            {
                DeleteScript(name);
            };

		// Open a script file in the default editor
        scriptCallbacks.onOpen = [](const std::string& name)
            {
                std::string headerPath = PathManager::Resolve("Game/GameCode/" + name + ".h");
                ShellExecuteA(
                    nullptr, "open",
                    headerPath.c_str(),
                    nullptr, nullptr, SW_SHOWNORMAL
                );
                DBG("EditorApp: Opening %s in default editor", name.c_str());
            };

        m_scriptsPanel.Render(scriptCallbacks, m_pScene.get());
    }

	// Render the Hierarchy panel with its callbacks
    {
		HierarchyPanel::Callbacks hierarchyCallbacks;

		// Callback for creating and adding a new actor in the scene
        hierarchyCallbacks.onCreateActor = [this](const std::string& name)
            {
                if (!m_pScene) return;

                Actor::InitDesc desc;
                desc.name = name;

                const bool succeeded = m_commandHistory.Execute(
                    std::make_unique<CreateActorCommand>(m_pScene.get(), desc)
                );

                if (succeeded)
                {
                    DBG(
                        "EditorApp: Created new actor '%s' through command history.",
                        name.c_str());
                }
                else
                {
                    DBG(
                        "EditorApp: Failed to create actor '%s'.",
                        name.c_str());
                }			};

		// Callback for deleting an actor from the scene
        hierarchyCallbacks.onDeleteActor = [this](Actor* actor)
            {
                if (m_pScene)
                {
					m_pScene->RemoveActor(actor);
					DBG("EditorApp: Deleted actor '%s' from scene", actor ? actor->GetName().c_str() : "Unknown");
                }
			};

        m_hierarchyPanel.Render(m_pScene.get(), hierarchyCallbacks);
    }

    m_inspectorPanel.Render(m_hierarchyPanel.GetSelectedActor());

	// Render scene view panel with the scene's color render target
	// Get the scene color render target from the engine
	GpuTexture* sceneColor = m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::SceneColor);
    
    if (sceneColor)
    {
		// Get the SRV index of the scene color render target and its GPU descriptor handle
		const uint32_t srvIndex = sceneColor->GetSrvIndex();

		// Get the GPU descriptor handle for the SRV
		const auto gpuHandle = m_pEngine->GetDescriptorHeapAllocator()->GetCbvSrvUavGpuHandle(srvIndex);

		// Render the scene view panel with the scene color render target
        m_sceneViewPanel.Render(gpuHandle);
    }

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
