#include <windows.h>
#include <algorithm>
#include <mmsystem.h>
#include <tchar.h>
#include <shellapi.h> 
#include "Core/EditorApp.h"
#include "Engine/Input/keyboard.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Window/WindowInfo.h"
#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"
#include "Core/EditorScene.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/Collider.h"
#include "Engine/UI/Canvas.h"
#include "Engine/UI/UIRenderer.h"
#include "Engine/UI/UIimage.h"
#include "Engine/Scene/SceneLoader.h"
#include "Engine/Scene/SceneWriter.h"
#include "Engine/Core/Debug/Debug.h"
#include "Tools/BehaviorTemplateGenerator.h"
#include "Tools/ClassTemplateGenerator.h"
#include "Tools/ProjectBuilder.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Core/Path/PathManager.h"
#include "Command/RenameActorCommand.h"
#include "Command/CreateActorCommand.h"
#include "Command/DeleteActorCommand.h"
#include "Command/ReparentActorCommand.h"
#include "Command/AddComponentCommand.h"
#include "Command/RemoveComponentCommand.h"
#include "Command/TransformEditCommand.h"
#include "UI/EditorTheme.h"
#include "UI/Inspector/Components/TransformInspector.h"
#include "UI/Inspector/Components/MeshRendererInspector.h"
#include "UI/Inspector/Components/SpriteRendererInspector.h"
#include "UI/Inspector/Components/RectTransformInspector.h"
#include "UI/Inspector/Components/ColliderInspector.h"
#include "UI/Inspector/Components/CameraInspector.h"
#include "UI/Inspector/Components/CanvasInspector.h"
#include "UI/Inspector/Components/UIRendererInspector.h"
#include "UI/Inspector/Components/UIImageInspector.h"
#include "Scene/ScenePicker.h"

#pragma comment(lib, "winmm.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

    CreateMainWindow();             // Create main window
    PrepareInstance();              // Prepare instance
    InitInstance();                 // Initialize instance
    InitImGui();                    // Initialize ImGui
	RegisterComponentInspectors();  // Register component inspectors for the editor

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
    // Cancel ongoing editing transactions
    CancelTransformEdit();
    CancelRectTransformEdit();

    // Clear inspector info to avoid dangling pointers to the soon-to-be-destroyed scene's actors/components
    m_hierarchyPanel.ClearSelection();

	// Clear the canvas edit context to avoid dangling pointers
    m_canvasEditContext.Clear();

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

	ApplyCurrentViewportSizeToScene();

    DBG("EditorApp: New scene created.");
}

// Load a scene from a file path
void EditorApp::LoadScene(const std::string& filePath)
{
    // Cancel ongoing editing transactions
    CancelTransformEdit();
    CancelRectTransformEdit();

    // Clear inspector info to avoid dangling pointers to the soon-to-be-destroyed scene's actors/components
    m_hierarchyPanel.ClearSelection();

	// Clear the canvas edit context to avoid dangling pointers
    m_canvasEditContext.Clear();

    // Clear the command history
    m_commandHistory.Clear();

	// Create a new scene instance and initialize it
    m_pScene = std::make_unique<EditorScene>();
    m_pScene->Initialize(m_engineContext);

	// Load the scene data from file
    bool result = SceneLoader::LoadScene(filePath, m_pScene.get());

    if (result) DBG("EditorApp: Loaded scene from %s", filePath.c_str());
    else        DBG("EditorApp: Failed to load scene from %s", filePath.c_str());

	// Set the viewport size based on the current scene color render target
    ApplyCurrentViewportSizeToScene();
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

	// Cancel ongoing editing transactions
	CancelTransformEdit();
	CancelRectTransformEdit();

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
    WindowInfo::Get().SetWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);

    m_pEngine         = std::make_unique<Engine>();
    m_pRenderer       = std::make_unique<Renderer>();
    m_pTextureManager = std::make_unique<TextureManager>();
    m_pMeshManager    = std::make_unique<MeshManager>();
	m_pAssetManager   = std::make_unique<AssetManager>();

    m_engineContext = {
        m_pRenderer.get(),
        m_pTextureManager.get(),
        m_pMeshManager.get(),
        m_pAssetManager.get()
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
	m_pAssetManager->Initialize(
        PathManager::Resolve("asset"),
        m_pTextureManager.get(),
        m_pMeshManager.get()
    );
    m_pEngine->InitBindings(m_pTextureManager.get());
    m_pRenderer->Initialize(pDevice, m_pEngine->GetDescriptorHeapAllocator(), m_pTextureManager.get(), m_pMeshManager.get());

    m_pEngine->BeginFrame();
    m_pEngine->RenderEnd();

    InputManager::GetInstance().Initialize();
}

void EditorApp::InitImGui()
{
	//--------------------------
	// Initialize ImGui context
	//--------------------------
    IMGUI_CHECKVERSION();

	//---------------------------
	// Apply custom editor theme
	//---------------------------
    ImGui::CreateContext();

    EditorTheme::ApplyStyle();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDpiScaleFonts = true;

	//---------------------------------
	// Load custom font for the editor
	//---------------------------------
    EditorFontConfig fontConfig;
    fontConfig.filePath = "C:/Windows/Fonts/Meiryo.ttc";
    fontConfig.sizePixels = 16.0f;
    fontConfig.fontIndex = 3;

    EditorTheme::LoadFont(io, fontConfig);

	//-------------------------------------------
	// Initialize ImGui for Win32 and DirectX 12
	//-------------------------------------------
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

void EditorApp::RegisterComponentInspectors()
{
    auto& registry =  m_inspectorPanel.GetComponentInspectorRegistry();

    registry.Register<Transform>(&TransformInspector::Draw);
	registry.Register<MeshRenderer>(&MeshRendererInspector::Draw);
	registry.Register<SpriteRenderer>(&SpriteRendererInspector::Draw);
	registry.Register<RectTransform>(&RectTransformInspector::Draw);
	registry.Register<Collider>(&ColliderInspector::Draw);
	registry.Register<Camera>(&CameraInspector::Draw);
	registry.Register<Canvas>(&CanvasInspector::Draw);
	registry.Register<UIRenderer>(&UIRendererInspector::Draw);
	registry.Register<UIImage>(&UIImageInspector::Draw);
}

void EditorApp::Update(float deltaTime)
{
    InputManager::GetInstance().Update();

	// Handle any pending resize requests for the scene view panel
    ApplySceneViewResizeRequest();

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

    const EditorViewportMode viewportMode = m_sceneViewPanel.GetViewMode();
    const bool isSceneView = viewportMode == EditorViewportMode::Scene;

	// Set up the render view policy based on the current viewport mode
    RenderViewPolicy viewPolicy{};

    // CanvasEditContext is a persistent edit scope. Initialize it from the
    // current selection only when no valid Canvas is already open.
    Canvas* editingCanvas = nullptr;

	// Canvas View mode requires a valid Canvas
    if (!isSceneView && m_pScene)
    {
		// Resolve the currently editing Canvas from the CanvasEditContext
        editingCanvas = m_canvasEditContext.ResolveCanvas(*m_pScene);

		// First selection of a Canvas in the hierarchy panel opens the CanvasEditContext for editing.
        if (!editingCanvas)
        {
            Actor* selectedActor = m_hierarchyPanel.GetSelectedActor(m_pScene.get());

            if (m_canvasEditContext.OpenFromActor(selectedActor))
            {
                editingCanvas = m_canvasEditContext.ResolveCanvas(*m_pScene);
            }
        }

        // Sync the CanvasViewNavigation state with the currently editing Canvas
        SyncCanvasViewNavigation(editingCanvas);
    }

    // Set up the policy
    if (isSceneView)
    {
		// Only render the world-space objects in the scene view (no screen-space objects)
        viewPolicy.renderSpaceFilter = RenderSpaceFilter::WorldOnly;
    }
    else
    {
		// In canvas view, render all objects (world-space and screen-space) if editing a canvas,
        viewPolicy.renderSpaceFilter = editingCanvas
            ? RenderSpaceFilter::All : RenderSpaceFilter::ScreenOnly;

		// Set the root canvas for rendering in canvas view (accepts nullptr if no canvas is selected)
        viewPolicy.canvasViewRoot = editingCanvas;
    }


	// Build CameraInfo based on the current viewport mode and size
    GpuTexture* sceneColor = m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::SceneColor);
    if (!sceneColor) return;

    const CameraInfo viewportCameraInfo = BuildViewportCameraInfo(sceneColor->GetWidth(), sceneColor->GetHeight());

    if (m_pScene)
    {
        m_pScene->OnRender(m_engineContext, &viewportCameraInfo, viewPolicy);
    }

    const RenderSpace targetRenderSpace = isSceneView
        ? RenderSpace::World : RenderSpace::Screen;

    // Build render data for the selected object in the scene view
    // (for outline rendering)
    BuildSelectionRenderData(targetRenderSpace, viewportCameraInfo, editingCanvas);

    // Scene View only requires shadow rendering
    if (isSceneView)
    {
        RenderPassTarget shadowTarget
        {
            RenderPassTargetType::DepthOnly,
            RenderPassTarget::InvalidIndex,
            static_cast<uint32_t>(Engine::BuiltinRenderTarget::ShadowMap)
        };

        m_pEngine->BeginPass(shadowTarget);
        m_pRenderer->RenderShadowMap(m_pEngine->GetCommandList());
        m_pEngine->EndPass(shadowTarget);
    }

    // Both Scene View and Canvas View use SceneColor as their display texture
    RenderPassTarget sceneTarget
    {
        RenderPassTargetType::ColorDepth,
        static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneColor),
        static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneDepth)
    };

    m_pEngine->BeginPass(sceneTarget);

    if (isSceneView)
    {
        // Render world-space objects using the Editor Camera
        GpuTexture* shadowMap = m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::ShadowMap);

        if (shadowMap)
        {
            m_pRenderer->RenderScene(m_pEngine->GetCommandList(), shadowMap->GetSrvIndex());
        }
    }
    else
    {
        // Render only screen-space objects using the viewport-based
        // orthographic projection created by RenderScreenSpace()
        GpuTexture* sceneColor = m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::SceneColor);

        if (sceneColor)
        {
            m_pRenderer->RenderScreenSpace(
                m_pEngine->GetCommandList(),
                sceneColor->GetWidth(),
                sceneColor->GetHeight(),
                RenderTargetFormat::HDR,
				&viewportCameraInfo
            );
        }
    }

    m_pEngine->EndPass(sceneTarget);

	// Render the selection mask for the selected object in the scene view
    RenderPassTarget selectionMaskTarget
    {
        RenderPassTargetType::ColorDepth,
        static_cast<uint32_t>(Engine::BuiltinRenderTarget::SelectionMask),
        static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneDepth),
        true,
        false
    };

    m_pEngine->BeginPass(selectionMaskTarget);
    m_pRenderer->RenderSelectionMask(m_pEngine->GetCommandList(),m_selectionRenderData);
    m_pEngine->EndPass(selectionMaskTarget);

    // Get the selection mask render target for outline rendering
    GpuTexture* selectionMask = m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::SelectionMask);

	// Render the selection outline for the selected object in the scene view
    RenderPassTarget selectionOutlineTarget
    {
        RenderPassTargetType::ColorDepth,
        static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneColor),
        static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneDepth),
        false,	// Preserve the rendered scene color
        false	// Preserve the scene depth
    };

    m_pEngine->BeginPass(selectionOutlineTarget);
    m_pRenderer->RenderSelectionOutline(m_pEngine->GetCommandList(), selectionMask);
    m_pEngine->EndPass(selectionOutlineTarget);

	// Render the scene color render target to the back buffer (screen) for display
    RenderPassTarget backBufferTarget
    {
        RenderPassTargetType::BackBuffer,
        m_pEngine->GetCurrentBufferIndex(),
        RenderPassTarget::InvalidIndex
    };

	// Render ImGui on top of the back buffer
    m_pEngine->BeginPass(backBufferTarget);
    RenderImGui();
    m_pEngine->EndPass(backBufferTarget);

    m_pEngine->RenderEnd();
}

void EditorApp::RenderImGui()
{
    RenderMenuBar();
    RenderScriptsPanel();
    RenderHierarchyPanel();
    RenderInspectorPanel();
    RenderSceneViewPanel();

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(
        ImGui::GetDrawData(),
        m_pEngine->GetCommandList()
    );
}

void EditorApp::RenderMenuBar()
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
            if (m_pScene &&
                !SceneWriter::SaveScene(kDefaultScenePath, m_pScene.get()))
            {
                DBG("EditorApp: Save failed.");
            }
        };

    callbacks.onUndo = [this]()
        {
            // Cancel the current editing transaction
			CancelTransformEdit();
			CancelRectTransformEdit();

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
            // Cancel the current editing transaction
            CancelTransformEdit();
            CancelRectTransformEdit();

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
            const bool generated = isBehavior
                ? BehaviorTemplateGenerator::Generate(name)
                : ClassTemplateGenerator::Generate(name);

            if (generated)
            {
                DBG("EditorApp: Generated %s template '%s'",
                    isBehavior ? "Behavior" : "class", name.c_str());

                ReloadGameCode(true);
            }
        };

    callbacks.canUndo = m_commandHistory.CanUndo();
    callbacks.canRedo = m_commandHistory.CanRedo();

    m_menuBar.Render(callbacks);
}

void EditorApp::RenderScriptsPanel()
{
    ScriptsPanel::Callbacks callbacks;

    callbacks.onDelete = [this](const std::string& name)
        {
            DeleteScript(name);
        };

    callbacks.onOpen = [](const std::string& name)
        {
            const std::string headerPath =
                PathManager::Resolve("Game/GameCode/" + name + ".h");

            ShellExecuteA(
                nullptr,
                "open",
                headerPath.c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL
            );

            DBG("EditorApp: Opening %s in default editor", name.c_str());
        };

    m_scriptsPanel.Render(callbacks, m_pScene.get());
}

void EditorApp::RenderHierarchyPanel()
{
    HierarchyPanel::Callbacks callbacks;

	callbacks.onRenameActor = [this](const Guid& targetActorGuid, const std::string& newName) -> bool
		{
			if (!m_pScene) return false;

			Actor* actor = m_pScene->ResolveActor(targetActorGuid);
			if (!actor) return false;

			const std::string oldName = actor->GetName();
			const bool succeeded = m_commandHistory.Execute(
				std::make_unique<RenameActorCommand>(
					m_pScene.get(),
                    targetActorGuid,
					newName
				)
			);
			if (succeeded)
			{
				DBG(
					"EditorApp: Renamed Actor '%s' to '%s' through command history.",
					oldName.c_str(),
					newName.c_str()
				);
			}
			else
			{
				DBG(
					"EditorApp: Failed to rename Actor '%s' to '%s'.",
					oldName.c_str(),
					newName.c_str()
				);
			}
			return succeeded;
		};

    callbacks.onCreateActor = [this](const std::string& name, const Guid& parentGuid)
        {
            if (!m_pScene) return;

            Actor::InitDesc desc;
            desc.name = name;

            const bool succeeded = m_commandHistory.Execute(
                std::make_unique<CreateActorCommand>(m_pScene.get(), desc, parentGuid)
            );

            if (succeeded)
            {
                DBG(
                    "EditorApp: Created new actor '%s' through command history.",
                    name.c_str()
                );
            }
            else
            {
                DBG(
                    "EditorApp: Failed to create actor '%s'.",
                    name.c_str()
                );
            }
        };

    callbacks.onDeleteActor =
        [this](const Guid& actorGuid) -> bool
        {
			if (!m_pScene || !actorGuid.IsValid()) return false;

			Actor* actor = m_pScene->ResolveActor(actorGuid);
			if (!actor || actor->IsDestroyed()) return false;

            const std::string actorName = actor->GetName();

            const bool succeeded = m_commandHistory.Execute(
                std::make_unique<DeleteActorCommand>(
                    m_pScene.get(),
					actorGuid
                )
            );

            if (succeeded)
            {
                DBG("EditorApp: Deleted Actor '%s' through command history.", actorName.c_str());
            }
            else
            {
                DBG("EditorApp: Failed to delete Actor '%s'.", actorName.c_str());
            }

            return succeeded;
        };

    callbacks.onReparentActor =
        [this](const Guid& actorGuid, const Guid& newParentGuid) -> bool
        {
			if (!m_pScene || !actorGuid.IsValid()) return false;

			Actor* actor = m_pScene->ResolveActor(actorGuid);
			if (!actor || actor->IsDestroyed()) return false;

			Actor* newParent = newParentGuid.IsValid()
				? m_pScene->ResolveActor(newParentGuid) : nullptr;

			if (newParentGuid.IsValid() &&
				(!newParent || newParent->IsDestroyed()))
			{
				return false;
			}

			// Get name for debug logging
            const std::string actorName = actor->GetName();
            const std::string parentName = newParent
                ? newParent->GetName() : "Scene Root";

			// Execute the reparenting command through the command history
            const bool succeeded =
                m_commandHistory.Execute(
                    std::make_unique<ReparentActorCommand>(
                        m_pScene.get(),
						actorGuid,
						newParentGuid
                    )
                );

            if (succeeded)
            {
                DBG("EditorApp: Reparented Actor '%s' to '%s' through command history.", actorName.c_str(), parentName.c_str());
            }
            else
            {
                DBG("EditorApp: Failed to reparent Actor '%s' to '%s'.", actorName.c_str(), parentName.c_str());
            }

            return succeeded;
        };

	callbacks.onOpenCanvas =
		[this](const Guid& actorGuid)
		{
			if (!m_pScene || !actorGuid.IsValid()) return;

			Actor* actor = m_pScene->ResolveActor(actorGuid);
			if (!actor || actor->IsDestroyed() || !actor->GetComponentByClass<Canvas>())
			{
				return;
			}

            // Open the CanvasActor in the Canvas View by setting the Actor in the context
			if (m_canvasEditContext.OpenFromActor(actor))
			{
				// Set the scene view panel to Canvas mode when a canvas is opened
				m_sceneViewPanel.SetViewMode(EditorViewportMode::Canvas);
			}
		};

    m_hierarchyPanel.Render(m_pScene.get(), callbacks);
}

void EditorApp::RenderInspectorPanel()
{
    InspectorContext context;
	context.assetManager = m_pAssetManager.get();

	// Transform editing callbacks
	context.onTransformEditBegin = [this](const Guid& actorGuid, const Transform3D& before) { BeginTransformEdit(actorGuid, before);};
    context.onTransformEditEnd = [this](const Guid& actorGuid, const Transform3D& after) { EndTransformEdit(actorGuid, after);};
    context.onCancelTransformEdit = [this](){ CancelTransformEdit(); };

	// RectTransform editing callbacks
	context.onRectTransformEditBegin = [this](const Guid& actorGuid, const RectTransformEditState& before) { BeginRectTransformEdit(actorGuid, before); };
    context.onRectTransformEditEnd = [this](const Guid& actorGuid, const RectTransformEditState& after) { EndRectTransformEdit(actorGuid, after); };
    context.onCancelRectTransformEdit = [this]() { CancelRectTransformEdit(); };

    InspectorPanel::Callbacks callbacks;

	// Callback for adding a component to an actor.
    callbacks.onAddComponent =
        [this](const Guid& actorGuid, const std::string& componentName)
        {
            if (!m_pScene) return false;

            return m_commandHistory.Execute(
                std::make_unique<AddComponentCommand>(
                    m_pScene.get(),
                    actorGuid,
                    componentName
                )
            );
        };

	// Callback for removing a component from an actor.
    callbacks.onRemoveComponent =
        [this](
            const Guid& actorGuid,
            const std::string& componentName,
            std::size_t occurrenceIndex)
        {
            if (!m_pScene) return false;

            return m_commandHistory.Execute(
                std::make_unique<RemoveComponentCommand>(
                    m_pScene.get(),
                    actorGuid,
                    componentName,
                    occurrenceIndex
                )
            );
        };

	m_inspectorPanel.Render(m_hierarchyPanel.GetSelectedActor(m_pScene.get()), context, callbacks);
}

void EditorApp::RenderSceneViewPanel()
{
	// Get the scene color render target from the engine
    GpuTexture* sceneColor =  m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::SceneColor);

    if (!sceneColor) return;

	// Get the GPU descriptor handle for the scene color render target's SRV
    const uint32_t srvIndex = sceneColor->GetSrvIndex();
    const auto gpuHandle = m_pEngine->GetDescriptorHeapAllocator()->GetCbvSrvUavGpuHandle(srvIndex);


	// Build the overlay data for the scene view panel based on the current view mode and any selected screen canvas
    ViewportOverlayData overlayData = BuildViewportOverlayData(sceneColor->GetWidth(), sceneColor->GetHeight());

	// Render the scene view panel with the scene color render target
    m_sceneViewPanel.Render(
        gpuHandle,
        sceneColor->GetWidth(),
        sceneColor->GetHeight(),
		overlayData
    );

    // Handle the user manipulation in the Canvas View
    CanvasNavigationInput navigationInput;

    if (m_sceneViewPanel.ConsumeCanvasNavigationInput(navigationInput))
    {
        ApplyCanvasNavigationInput(
            navigationInput,
            sceneColor->GetWidth(),
            sceneColor->GetHeight()
        );
    }

	// Handle the opening Canvas in the Canvas View mode if the user clicks a Canvas in the breadcrumb list
    Guid canvasActorGuid;

    if (m_sceneViewPanel.ConsumeCanvasOpenRequest(canvasActorGuid))
    {
        Actor* canvasActor = m_pScene
            ? m_pScene->ResolveActor(canvasActorGuid) : nullptr;

        if (canvasActor && !canvasActor->IsDestroyed() && canvasActor->GetComponentByClass<Canvas>())
        {
            m_canvasEditContext.OpenFromActor(canvasActor);
        }
    }

	// Handle the click event for the scene view panel to select an actor in the scene

	Vector2 pickUV; // UV coordinates of the click within the scene view panel

	// Check if the click event occurred and get the UV coordinates
	// of the click within the scene view panel if requested
    if (!m_sceneViewPanel.ConsumePickRequest(pickUV)) return;

	// Validate necessary pointers before proceeding with picking
	if (!m_pScene || !m_pEditorCamera) return;

	// Build the camera info for picking based on the current view mode (Scene or Canvas)
    const CameraInfo pickCameraInfo = BuildViewportCameraInfo(sceneColor->GetWidth(), sceneColor->GetHeight());

	// Determine the render space for picking based on the current view mode
	// World(3D) for Scene View, Screen(2D) for Canvas View
    const RenderSpace targetRenderSpace =
        m_sceneViewPanel.GetViewMode() == EditorViewportMode::Scene
        ? RenderSpace::World : RenderSpace::Screen;

	// Resolve the canvas for picking if in Canvas View mode
    Canvas* editingCanvas = nullptr;

    if (m_sceneViewPanel.GetViewMode() == EditorViewportMode::Canvas)
    {
        editingCanvas = m_canvasEditContext.ResolveCanvas(*m_pScene);
    }

    // Get the picked Actor information
    const std::optional<ScenePickHit> hit = ScenePicker::Pick(*m_pScene, pickCameraInfo, pickUV, targetRenderSpace, editingCanvas);

    if (hit)
    {
        m_hierarchyPanel.SelectActor(hit->actorGuid);
    }
    else
    {
        m_hierarchyPanel.ClearSelection();
    }
}

void EditorApp::ShutdownImGui()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void EditorApp::ApplySceneViewResizeRequest()
{
	UINT width = 0, height = 0;

	// Check if the scene view panel has requested a resize of the render target
    if (!m_sceneViewPanel.ConsumeResizeRequest(width, height)) return;
    
	// Resize the scene render targets (color and depth) to the new width and height
    if (!m_pEngine->ResizeSceneRenderTargets(width, height)) return;

	// Update the editor camera's lens parameters
	CameraLens lens = m_pEditorCamera->GetCameraLens();
	lens.width = static_cast<float>(width);
	lens.height = static_cast<float>(height);

	m_pEditorCamera->SetCameraLens(lens);

    // Apply the viewport size to the scene and invalidate
    // all layout elements affected by the size change
	if (m_pScene) m_pScene->SetViewportSize(width, height);
}

void EditorApp::ApplyCurrentViewportSizeToScene()
{
    if (!m_pScene || !m_pEngine) return;

    GpuTexture* sceneColor = m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::SceneColor);

    if (!sceneColor) return;

    m_pScene->SetViewportSize(
        sceneColor->GetWidth(),
        sceneColor->GetHeight()
    );
}

void EditorApp::BeginTransformEdit(const Guid& actorGuid, const Transform3D& before)
{
	// Validate the actorGuid
    if (!m_pScene || !actorGuid.IsValid())
    {
		CancelTransformEdit();
        return;
    }

	Actor* actor = m_pScene->ResolveActor(actorGuid);

    std::type_index transformType = std::type_index(typeid(Transform));
    Component* component = actor ? actor->GetComponentByExactType(transformType, 0) : nullptr;

    Transform* transform = dynamic_cast<Transform*>(component);

	if (!transform)
	{
		CancelTransformEdit();
		return;
	}

	// Stop any ongoing edit transaction
	CancelTransformEdit();
	CancelRectTransformEdit();

	// Store the actorGuid and the "before" transform in a new TransformEditTransaction
    m_transformEditTransaction = TransformEditTransaction{actorGuid, before};
}

void EditorApp::EndTransformEdit(const Guid& actorGuid, const Transform3D& after)
{
	// Validate that there is an ongoing transform edit transaction and that the actorGuid matches
    if (!m_transformEditTransaction) return;

    if (m_transformEditTransaction->actorGuid != actorGuid)
    {
		CancelTransformEdit();
        return;
    }

	// Store the "before" transform from the transaction before resetting it
    const Transform3D before = m_transformEditTransaction->before;

	// Do not create a command if the before and after states are identical (no change)
	if (before == after)
	{
		CancelTransformEdit();
		return;
	}

	// Reset the transaction to indicate that the transform edit has ended
    m_transformEditTransaction.reset();

	// Add a TransformEditCommand to the command history and execute it
    m_commandHistory.Execute(
        std::make_unique<TransformEditCommand>(
            m_pScene.get(),
            actorGuid,
            before,
            after
        )
    );
}

void EditorApp::CancelTransformEdit()
{
	if (!m_transformEditTransaction) return;

	const Guid actorGuid = m_transformEditTransaction->actorGuid;

    if (!m_pScene || !actorGuid.IsValid())
    {
        m_transformEditTransaction.reset();
        return;
    }

	Actor* actor = m_pScene->ResolveActor(actorGuid);

	std::type_index transformType = std::type_index(typeid(Transform));
	Component* component = actor ? actor->GetComponentByExactType(transformType, 0) : nullptr;

	Transform* transform = dynamic_cast<Transform*>(component);

	if (!transform)
    {
        m_transformEditTransaction.reset();
        return;
    }

	// Reset the Transform to its "before" state from the edit transaction
	transform->SetLocalTransform(m_transformEditTransaction->before);

	m_transformEditTransaction = {};
}

void EditorApp::BeginRectTransformEdit(const Guid& actorGuid, const RectTransformEditState& before)
{
	if (!m_pScene || !actorGuid.IsValid())
	{
		CancelRectTransformEdit();
		return;
	}

	Actor* actor = m_pScene->ResolveActor(actorGuid);

	std::type_index rectTransformType = std::type_index(typeid(RectTransform));
	Component* component = actor ? actor->GetComponentByExactType(rectTransformType, 0) : nullptr;

	RectTransform* rectTransform = dynamic_cast<RectTransform*>(component);

	if (!rectTransform)
	{
		CancelRectTransformEdit();
		return;
	}

    // Stop any ongoing transform edit transaction
    CancelTransformEdit();
    CancelRectTransformEdit();

	m_rectTransformEditTransaction = RectTransformEditTransaction{ actorGuid, before };
}

void EditorApp::EndRectTransformEdit(const Guid& actorGuid, const RectTransformEditState& after)
{
    if (!m_rectTransformEditTransaction) return;

	if (m_rectTransformEditTransaction->actorGuid != actorGuid)
	{
		CancelRectTransformEdit();
		return;
	}

	const RectTransformEditState before = m_rectTransformEditTransaction->before;

	// Do not create a command if the before and after states are identical (no change)
	if (before == after)
	{
		CancelRectTransformEdit();
		return;
	}

    m_rectTransformEditTransaction.reset();

	m_commandHistory.Execute(
        std::make_unique<RectTransformEditCommand>(
		    m_pScene.get(),
		    actorGuid,
		    before,
		    after
	    )
	);
}

void EditorApp::CancelRectTransformEdit()
{
	if (!m_rectTransformEditTransaction) return;

    const Guid actorGuid = m_rectTransformEditTransaction->actorGuid;

    if (!m_pScene || !actorGuid.IsValid())
    {
        m_rectTransformEditTransaction.reset();
        return;
    }

	Actor* actor = m_pScene->ResolveActor(actorGuid);

	std::type_index rectTransformType = std::type_index(typeid(RectTransform));
	Component* component = actor ? actor->GetComponentByExactType(rectTransformType, 0) : nullptr;

	RectTransform* rectTransform = dynamic_cast<RectTransform*>(component);

	if (!rectTransform)
    {
        m_rectTransformEditTransaction.reset();
        return;
    }

	// Reset the RectTransform to its "before" state from the edit transaction
	m_rectTransformEditTransaction->before.ApplyTo(*rectTransform);

	m_rectTransformEditTransaction = {};
}

void EditorApp::BuildSelectionRenderData(
    RenderSpace targetRenderSpace,
    const CameraInfo& viewportCameraInfo,
    const Canvas* canvasViewRoot
)
{
	// Clear any existing selection render data
    m_selectionRenderData.Clear();

    if (!m_pScene) return;

	// Get the currently selected actor from the hierarchy panel and validate it
	Actor* selectedActor = m_hierarchyPanel.GetSelectedActor(m_pScene.get());

    if (!selectedActor ||
        !selectedActor->IsActive() ||
        selectedActor->IsDestroyed())
    {
        return;
    }

	// Determine if the current view mode is Canvas View based on whether a canvas view root is provided
    const bool isCanvasView = canvasViewRoot != nullptr;

    const Matrix4x4 worldToCanvas = isCanvasView
        ? canvasViewRoot->GetContentWorldMatrix().Inverse()
        : Matrix4x4::Identity();

	// Try to get the MeshRenderer component from the selected actor
    if (MeshRenderer* meshRenderer = selectedActor->GetComponentByClass<MeshRenderer>())
    {
		// Validate the MeshRenderer component
        if (!meshRenderer->IsVisible() || !meshRenderer->IsConfigured())
        {
            return;
        }

		// In Canvas View, ensure that the renderer component is a descendant of the canvas view root
        if (isCanvasView)
        {
            if (!canvasViewRoot->ContainsRenderer(meshRenderer))
            {
                return;
            }
        }
        else if (meshRenderer->GetRenderSpace() != targetRenderSpace)
        {
            return;
        }

		// Get the render proxy
        const MeshRendererProxy& proxy = meshRenderer->GetRenderProxy();

		// Create render items for each submesh in the MeshRenderer
        for (const SubmeshRenderTemplate& renderTemplate : meshRenderer->GetRenderTemplates())
        {
            MeshRenderItem item = RenderSystem::CreateMeshRenderItem(renderTemplate, proxy);

			// In Canvas View, transform the world matrix of the render item to canvas space
			if (isCanvasView) item.common.worldMatrix *= worldToCanvas;

            m_selectionRenderData.AddMeshs(std::move(item));
        }

        return;
    }

	// Try to get the SpriteRenderer component from the selected actor
    if (SpriteRenderer* spriteRenderer = selectedActor->GetComponentByClass<SpriteRenderer>())
    {
		// Validate the SpriteRenderer component
        if (!spriteRenderer->IsVisible() || !spriteRenderer->IsConfigured())
        {
            return;
        }

        // In Canvas View, ensure that the renderer component is a descendant of the canvas view root
        if (isCanvasView)
        {
            if (!canvasViewRoot->ContainsRenderer(spriteRenderer))
            {
                return;
            }
        }
        else if (spriteRenderer->GetRenderSpace() != targetRenderSpace)
        {
            return;
        }


		// Get the render proxy
        const SpriteRendererProxy& proxy = spriteRenderer->GetRenderProxy(viewportCameraInfo);

		// Create a render item for the sprite
        SpriteRenderItem item = RenderSystem::CreateSpriteRenderItem(spriteRenderer->GetRenderTemplate(), proxy);

		// In Canvas View, transform the world matrix of the render item to canvas space
		if (isCanvasView) item.common.worldMatrix *= worldToCanvas;

        m_selectionRenderData.AddSprites(std::move(item));
    }

	// Try to get the UIRenderer component from the selected actor
	if (UIRenderer* uiRenderer = selectedActor->GetComponentByClass<UIRenderer>())
	{
		// Validate the UIRenderer component
        if (!uiRenderer->IsVisible() || !uiRenderer->IsConfigured())
        {
            return;
        }

        // In Canvas View, ensure that the renderer component is a descendant of the canvas view root
        if (isCanvasView)
        {
            if (!canvasViewRoot->ContainsRenderer(uiRenderer))
            {
                return;
            }
        }
        else if (uiRenderer->GetRenderSpace() != targetRenderSpace)
        {
            return;
        }


        // Get the render proxy
		const UIRendererProxy& proxy = uiRenderer->GetRenderProxy();

		// Create render items for each UI element in the UIRenderer
        for (const UIRenderElement& element : uiRenderer->GetRenderTemplate())
        {
            UIRenderItem item = RenderSystem::CreateUIRenderItem(element, proxy);

            // In Canvas View, transform the world matrix of the render item to canvas space
            if (isCanvasView) item.common.worldMatrix *= worldToCanvas;

            m_selectionRenderData.AddUI(std::move(item));
        }
	}
}

CameraInfo EditorApp::BuildViewportCameraInfo(UINT viewportWidth, UINT viewportHeight) const
{
	// Avoid zero division and invalid viewport sizes
    if (viewportWidth == 0 || viewportHeight == 0) return {};

	// Decide projection type based on the current viewport mode (Scene or Canvas)

	if (m_sceneViewPanel.GetViewMode() == EditorViewportMode::Scene)
	{
		// Use the editor camera's perspective projection for Scene View
		return m_pEditorCamera->GetCameraInfo();
	}

	// From here on, build an orthographic projection for Canvas View
    // based on the canvas reference size and viewport size

    const Vector2 fitExtent = CalculateCanvasViewExtent(viewportWidth, viewportHeight);
    const Vector2 visibleExtent = fitExtent / m_canvasViewNavigation.zoom;

    CameraInfo cameraInfo{};
    cameraInfo.position = { 0.0f, 0.0f, -1.0f };
    cameraInfo.forward = Vector3::Forward();
    cameraInfo.up = Vector3::Up();

    cameraInfo.viewMatrix =
        Matrix4x4::CreateTranslation(
            {
                -m_canvasViewNavigation.center.x,
                -m_canvasViewNavigation.center.y,
                0.0f
            }
        );

	// Create an orthographic projection matrix for the camera
    // based on the calculated view width and height
    cameraInfo.projMatrix =
        Matrix4x4::CreateOrthographic(
            visibleExtent.x,
            visibleExtent.y,
            -1.0f,
            100.0f
        );

    return cameraInfo;
}

ViewportOverlayData EditorApp::BuildViewportOverlayData(UINT viewportWidth, UINT viewportHeight)
{
	ViewportOverlayData overlayData;

	if (!m_pScene || viewportWidth == 0 || viewportHeight == 0)
	{
		return overlayData;
	}

	const CameraInfo cameraInfo = BuildViewportCameraInfo(viewportWidth, viewportHeight);
	const Matrix4x4 viewProjection = cameraInfo.viewMatrix * cameraInfo.projMatrix;

	// Built overlay data for the Canvas View mode
	// In Canvas View, we need to display the editing canvas and ancestor Canvases of the selected Canvas up to the editing canvas
	if (m_sceneViewPanel.GetViewMode() == EditorViewportMode::Canvas)
	{
		// Resolve editing canvas from the CanvasEditContext, which is the root canvas for the Canvas View
		Canvas* editingCanvas = m_canvasEditContext.ResolveCanvas(*m_pScene);
		if (!editingCanvas) return overlayData;

		std::vector<Canvas*> breadcrumbPath;

		for (Actor* current = editingCanvas->GetOwner();
			current;
			current = current->GetParent())
		{
			if (Canvas* canvas = current->GetComponentByClass<Canvas>())
			{
				breadcrumbPath.push_back(canvas);
			}
		}

		std::reverse(breadcrumbPath.begin(), breadcrumbPath.end());

		const auto appendBreadcrumb =
			[&overlayData](Canvas* canvas)
			{
				Actor* canvasActor = canvas ? canvas->GetOwner() : nullptr;
				if (!canvasActor) return;

				CanvasBreadcrumbItem item;
				item.canvasActorGuid = canvasActor->GetGuid();
				item.name = canvasActor->GetName();

				overlayData.canvasBreadcrumbs.push_back(std::move(item));
			};

		constexpr size_t kVisibleParentCount = 3;   // Numberof the displaying parent Canvases in the breadcrumb list (excluding the root and editing canvas)
		constexpr size_t kMaxBreadcrumbCanvasCount = kVisibleParentCount + 2;	// Root + parents + editing Canvas

		if (breadcrumbPath.size() <= kMaxBreadcrumbCanvasCount)
		{// Within the limit, display all ancestor Canvases in the breadcrumb list
			for (Canvas* canvas : breadcrumbPath)
			{
				appendBreadcrumb(canvas);
			}
		}
		else
		{// Exceeds the limit, display the root Canvas, ellipsis, and the last few ancestor Canvases in the breadcrumb list
			// Display editing root Canvas
            appendBreadcrumb(breadcrumbPath.front());

			// Omit the middle ancestor Canvases and display ellipsis in the breadcrumb list
			CanvasBreadcrumbItem ellipsis;
			ellipsis.type = CanvasBreadcrumbItemType::Ellipsis;
			ellipsis.name = "...";
			overlayData.canvasBreadcrumbs.push_back(std::move(ellipsis));

			const size_t firstVisibleIndex = breadcrumbPath.size() - (kVisibleParentCount + 1);

			// Display the last few ancestor Canvases in the breadcrumb list
			for (size_t i = firstVisibleIndex; i < breadcrumbPath.size(); i++)
			{
				appendBreadcrumb(breadcrumbPath[i]);
			}
		}

		// Get reference size and inverse matrix of the editing Canvas for transforming world coordinates to the editing canvas's local space
		// The editing canvas is treated as the origin of the Canvas View
		const Vector2 referenceSize = editingCanvas->GetLayoutReferenceSize();
		const Matrix4x4 worldToEditingCanvas = editingCanvas->GetContentWorldMatrix().Inverse();

		// Lambda function to append a ViewportCanvasRect to the overlay data,
        // transforming the corners from local space to NDC space using the provided transform and view-projection matrix
		const auto appendRect =[&overlayData, &viewProjection](const Vector3 (&corners)[4], const Matrix4x4& transform, ViewportCanvasRole role)
			{
				ViewportCanvasRect rect;
				rect.role = role;

				for (size_t i = 0; i < 4; i++)
				{
					const Vector3 position = transform.TransformPoint(corners[i]);
					const Vector3 ndc = viewProjection.TransformPoint(position);
					rect.corners[i] = { ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f };
				}

				overlayData.canvasRects.push_back(rect);
			};

		// Original corners of the editing canvas in its local space, based on its reference size
		const Vector3 editingCorners[4]
		{
			{ -referenceSize.x * 0.5f, -referenceSize.y * 0.5f, 0.0f },
			{ -referenceSize.x * 0.5f,  referenceSize.y * 0.5f, 0.0f },
			{  referenceSize.x * 0.5f,  referenceSize.y * 0.5f, 0.0f },
			{  referenceSize.x * 0.5f, -referenceSize.y * 0.5f, 0.0f }
		};

		// Append the editing root canvas rect to the overlay data with identity transform and role as EditingRoot
		appendRect(editingCorners, Matrix4x4::Identity(), ViewportCanvasRole::EditingRoot);

		// Get the selected actor from the hierarchy panel and find its closest canvas for highlighting in the overlay
		Actor* selectedActor = m_hierarchyPanel.GetSelectedActor(m_pScene.get());
		Canvas* selectedCanvas = CanvasEditContext::FindClosestCanvas(selectedActor);

		// Collect the ancestor canvases of the selected canvas up to the editing canvas
        // for overlay rects if the selected canvas is different from the editing canvas
		if (selectedCanvas && selectedCanvas != editingCanvas && editingCanvas->ContainsCanvas(selectedCanvas))
		{
			std::vector<Canvas*> path;

			// Traverse the hierarchy from the selected canvas up to the editing canvas, collecting all ancestor canvases
			for (Actor* current = selectedCanvas->GetOwner(); current; current = current->GetParent())
			{
				if (Canvas* canvas = current->GetComponentByClass<Canvas>())
				{
					path.push_back(canvas);
					if (canvas == editingCanvas) break;
				}
			}

			// Reverse the path to have the editing canvas first and the selected canvas last
			std::reverse(path.begin(), path.end());

			const Vector3 unitCorners[4]
			{
				{ -0.5f, -0.5f, 0.0f }, { -0.5f, 0.5f, 0.0f },
				{  0.5f,  0.5f, 0.0f }, {  0.5f, -0.5f, 0.0f }
			};

			// Append overlay rects for each ancestor canvas in the path,
            // transforming their local rects to world space and then to the editing canvas's local space
			for (size_t i = 1; i < path.size(); i++)
			{
				Actor* canvasActor = path[i]->GetOwner();

				// Get RectTransform of the actor
				RectTransform* rect = canvasActor ? canvasActor->GetComponentByClass<RectTransform>() : nullptr;
				if (!rect) continue;

				// World space to editing canvas space transform
				Matrix4x4 transform = rect->GetWorldMatrix() * worldToEditingCanvas;

				// Determine role if the current Canvas is selected or an ancestor for overlay highlighting
				ViewportCanvasRole role = (i + 1 == path.size()) ? ViewportCanvasRole::Selected : ViewportCanvasRole::Ancestor;

                // Appending
				appendRect(unitCorners, transform, role);
			}
		}

		overlayData.referenceSize = referenceSize;
		overlayData.canvasOrder = editingCanvas->GetSortOrder();
		return overlayData;
	}

	// Build overlay data for the Scene View mode below
	// In Scene View, we need to display all world-space canvases in the scene and highlight the selected canvas if any

	// Get selected actor and its closest canvas if existing, for highlighting in the overlay
	Actor* selectedActor = m_hierarchyPanel.GetSelectedActor(m_pScene.get());
	Canvas* selectedCanvas = CanvasEditContext::FindClosestCanvas(selectedActor);

	const Vector3 localCorners[4]
	{
		{ -0.5f, -0.5f, 0.0f },
		{ -0.5f,  0.5f, 0.0f },
		{  0.5f,  0.5f, 0.0f },
		{  0.5f, -0.5f, 0.0f }
	};

	// Iterate through all actors in the scene to find world-space canvases and build overlay rects for them
	for (Actor* actor : m_pScene->GetAllActors())
	{
		if (!actor || !actor->IsActive() || actor->IsDestroyed()) continue;

		// Tray to get Canvas component from the current actor
		Canvas* canvas = actor->GetComponentByClass<Canvas>();

		// Check if the canvas exists and is a world-space canvas that is visible in the hierarchy
		if (!canvas || canvas->GetRenderMode() != CanvasRenderMode::WorldSpace || !canvas->IsHierarchyVisible())
		{
			continue;
		}

        // Determine the frame matrix for the canvas based on whether it is a root canvas or a child canvas
		Matrix4x4 frameMatrix;

		if (canvas->IsRootCanvas())
		{// Root Canvas
			const Vector2 referenceSize = canvas->GetLayoutReferenceSize();

			// Scaling the content world matrix (simple TRS matrix ignoring RectTransform property)
            // by the reference size to get the frame matrix for the root canvas
			frameMatrix = Matrix4x4::CreateScale({ referenceSize.x, referenceSize.y, 1.0f }) * canvas->GetContentWorldMatrix();
		}
		else
		{// Child Canvas
			RectTransform* rectTransform = actor->GetComponentByClass<RectTransform>();

			if (!rectTransform) continue;

			// Get the world matrix of the RectTransform which is based on
            // the hierarchy of UI elements and their layout properties
			frameMatrix = rectTransform->GetWorldMatrix();
		}

		ViewportCanvasRect rect;

		// Set the role of the overlay rect based on whether the canvas is the selected canvas or an ancestor
		rect.role = canvas == selectedCanvas
			? ViewportCanvasRole::Selected : ViewportCanvasRole::Ancestor;

		bool isInFrontOfCamera = true;  // Flag to check if the canvas is in front of the camera

		// Transform the local corners of the canvas to world space
        // and then to normalized device coordinates (NDC) for rendering the overlay rect
		for (size_t i = 0; i < 4; i++)
		{
			const Vector3 worldPosition = frameMatrix.TransformPoint(localCorners[i]);
			const Vector3 viewPosition = cameraInfo.viewMatrix.TransformPoint(worldPosition);

			// Avoid mirrored guides when a Canvas is behind or intersects the Editor Camera's near plane.
			if (viewPosition.z <= 0.001f)
			{
				isInFrontOfCamera = false;
				break;
			}

			const Vector3 ndc = viewProjection.TransformPoint(worldPosition);

			rect.corners[i] =
			{
				ndc.x * 0.5f + 0.5f,
				0.5f - ndc.y * 0.5f
			};
		}

		// Add the overlay rect to the overlay data only if the canvas is in front of the camera
		if (isInFrontOfCamera)
		{
			overlayData.worldCanvasRects.push_back(rect);
		}
	}

	return overlayData;
}

void EditorApp::SyncCanvasViewNavigation(const Canvas* editingCanvas)
{
	Actor* canvasActor = editingCanvas ? editingCanvas->GetOwner() : nullptr;

	const Guid canvasGuid = canvasActor ? canvasActor->GetGuid() : Guid{};

	// Check if the stored Canvas Guid for manipulation is the same as the current editing canvas.
	if (m_canvasViewNavigation.canvasActorGuid == canvasGuid) return;

	// Reset the canvas view navigation state when switching to a different canvas in the Canvas View mode
	m_canvasViewNavigation.canvasActorGuid = canvasGuid;
	m_canvasViewNavigation.center = Vector2::Zero();
	m_canvasViewNavigation.zoom = 1.0f;
}

Vector2 EditorApp::CalculateCanvasViewExtent(UINT viewportWidth, UINT viewportHeight) const
{
	// Fallback to viewport size if no scene or canvas is available
    Vector2 referenceSize =
    {
        static_cast<float>(viewportWidth),
        static_cast<float>(viewportHeight)
    };

	// Get the reference size of the editing canvas if available
    if (m_pScene)
    {
        Canvas* canvas = m_canvasEditContext.ResolveCanvas(*m_pScene);
        if (canvas) referenceSize = canvas->GetLayoutReferenceSize();
    }

	// Aspect ratiio of the viewport and the reference size of the canvas
    const float viewportAspect = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
    const float canvasAspect = referenceSize.x / referenceSize.y;

	// Calculate the extent which can fit the canvas in the viewport while maintaining the both aspect ratios

    Vector2 extent = referenceSize;

    if (viewportAspect > canvasAspect)
	{// Canvas has a taller aspect ratio than the viewport
        // Set the width when the viewport height is matched to the canvas height
        extent.x = extent.y * viewportAspect;
    }
    else
	{// Canvas has a wider aspect ratio than the viewport
		// Set the height when the viewport width is matched to the canvas width
        extent.y = extent.x / viewportAspect;
    }

    return extent;
}

void EditorApp::ApplyCanvasNavigationInput(const CanvasNavigationInput& input, UINT viewportWidth, UINT viewportHeight)
{
    // Fitting the viewport to Editing-Root Canvas (No pan and zoom)
    if (input.fitRequested)
    {
        m_canvasViewNavigation.center = Vector2::Zero();
        m_canvasViewNavigation.zoom = 1.0f;
        return;
    }

	// Avoid zero division and invalid viewport sizes
    if (viewportWidth == 0 || viewportHeight == 0)  return;

    // Get the extent without zoom
    const Vector2 fitExtent = CalculateCanvasViewExtent(viewportWidth, viewportHeight);

	// Calculate the extent considering the current zoom level
    const Vector2 currentExtent = fitExtent / m_canvasViewNavigation.zoom;

    // Calculate center position considering the amount of the pan manipulation
    m_canvasViewNavigation.center.x -= input.panDeltaPixels.x * currentExtent.x / static_cast<float>(viewportWidth);
    m_canvasViewNavigation.center.y += input.panDeltaPixels.y * currentExtent.y / static_cast<float>(viewportHeight);

	if (input.wheelDelta == 0.0f) return;   // End if there is no zoom input

	// Calculate the pivot offset before zooming
    const Vector2 pivotOffsetBefore
    {
        (input.zoomPivotUV.x - 0.5f) * currentExtent.x,
        (0.5f - input.zoomPivotUV.y) * currentExtent.y
    };

    constexpr float kMinZoom = 0.1f;
    constexpr float kMaxZoom = 8.0f;

	// Clamp the new zoom level within the defined range to prevent excessive zooming in or out
	// Use pow() to maintain the feeling of zooming manipulation with the mouse wheel
	// Make the change of zoom level by a wheel step multiplicative
    const float newZoom = std::clamp(m_canvasViewNavigation.zoom * std::pow(1.1f, input.wheelDelta), kMinZoom, kMaxZoom);

    // The extent after zooming
    const Vector2 newExtent = fitExtent / newZoom;

	// Calculate the pivot offset after zooming
    const Vector2 pivotOffsetAfter
    {
        (input.zoomPivotUV.x - 0.5f) * newExtent.x,
        (0.5f - input.zoomPivotUV.y) * newExtent.y
    };

	// Adjust the center position to keep the zoom pivot point fixed in the viewport
    m_canvasViewNavigation.center += pivotOffsetBefore - pivotOffsetAfter;

    m_canvasViewNavigation.zoom = newZoom;
}
