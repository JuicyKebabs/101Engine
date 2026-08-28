#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <mmsystem.h>
#include <tchar.h>
#include <shellapi.h> 
#include "Core/EditorApp.h"
#include "Engine/Input/keyboard.h"
#include "Engine/Input/InputManager.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"
#include "Core/EditorScene.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Graphics/SwapChain.h"
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
#include "Engine/Core/String/StringEncoding.h"
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
#include "Scene/SceneCloner.h"

#pragma comment(lib, "winmm.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static const int WINDOW_WIDTH  = 1280;
static const int WINDOW_HEIGHT = 720;

static const char* kDefaultScenePath = "asset/scenes/test.scene";

namespace
{
    constexpr const char* kHotReloadScenePath = "asset/scenes/_hotreload_temp.scene";
    constexpr const char* kActiveGameCodePath = "build/bin/Debug/GameCode.dll";
    constexpr const char* kStagedGameCodePath = "build/bin/Debug/GameCode.staged.dll";
    constexpr const char* kPreviousGameCodePath = "build/bin/Debug/GameCode.previous.dll";
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

    Window::InitDesc windowDesc{};
    windowDesc.className = L"101EngineEditorWindow";
    windowDesc.title = L"101Editor";
    windowDesc.clientWidth = WINDOW_WIDTH;
    windowDesc.clientHeight = WINDOW_HEIGHT;
    windowDesc.messageCallback = [](
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        LRESULT& outResult)
        {
            if (ImGui::GetCurrentContext() &&
                ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
            {
                outResult = TRUE;
                return true;
            }

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
        DBG("EditorApp: Failed to initialize the main window.");
        return false;
    }

    PrepareInstance();              // Prepare instance
    InitInstance();                 // Initialize instance
    InitImGui();                    // Initialize ImGui
	RegisterComponentInspectors();  // Register component inspectors for the editor

    NewScene();            // Start with a fresh scene (MainCamera-tagged DefaultCamera)

    return true;
}

void EditorApp::Run()
{
    m_window.Show();

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

                if (m_window.IsMinimized())
                {
                    m_timeManager.Update();
                    continue;
                }

                if (!ApplyWindowResizeRequest())
                {
                    continue;
                }

                // ImGui's NewFrame is called before Update so that debug
                // ImGui windows (e.g. EditorCamera's "Camera Info") can be
                // drawn from within Update().
                ImGui_ImplDX12_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                float deltaTime = m_timeManager.GetDeltaTime();
                m_timeManager.Update();

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

    if (m_pPlayScene)
    {
        m_pPlayScene->Finalize();
        m_pPlayScene.reset();
    }

    if (m_pEditScene)
    {
        m_pEditScene->Finalize();
        m_pEditScene.reset();
    }

    ComponentRegistry::Get().UnregisterAllGameComponents();
    if (m_hGameCodeDll)
    {
        FreeLibrary(m_hGameCodeDll);
        m_hGameCodeDll = nullptr;
    }

	// Terminate editor related resources
    ShutdownImGui();
    m_pEngine->Terminate();
    m_window.Terminate();
}

bool EditorApp::ApplyWindowResizeRequest()
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
		DBG("EditorApp: Failed to resize engine output to %ux%u", requestedSize.width, requestedSize.height);
		return false;
	}

	m_window.CommitResize();
	return true;
	
}

// Create a new scene with default settings
// (a single DefaultCamera actor tagged "MainCamera")
void EditorApp::NewScene()
{
    // Cancel ongoing editing transactions
    StopAllEditTransactions();

    // Clear inspector info to avoid dangling pointers to the soon-to-be-destroyed scene's actors/components
    m_hierarchyPanel.ClearSelection();

	// Clear the canvas edit context to avoid dangling pointers
    m_canvasEditContext.Clear();

	// Clear the command history
	m_commandHistory.Clear();

	// Create a new scene instance and initialize it
	if (m_pEditScene)
	{
		m_pEditScene->Finalize();
		m_pEditScene.reset();
	}

    m_pEditScene = std::make_unique<EditorScene>();
    m_pEditScene->Initialize(m_engineContext);

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
    m_pEditScene->AddRootActor(std::move(cameraActorOwned));
    m_pEditScene->GetCameraSystem()->SetMainCamera(camera);

	ApplySceneRenderTargetSizeToScene(*m_pEditScene);

    DBG("EditorApp: New scene created.");
}

// Load a scene from a file path
void EditorApp::LoadScene(const std::string& filePath)
{
    // Cancel ongoing editing transactions
    StopAllEditTransactions();

    // Clear inspector info to avoid dangling pointers to the soon-to-be-destroyed scene's actors/components
    m_hierarchyPanel.ClearSelection();

	// Clear the canvas edit context to avoid dangling pointers
    m_canvasEditContext.Clear();

    // Clear the command history
    m_commandHistory.Clear();

	// Create a new scene instance and initialize it
	if (m_pEditScene)
	{
		m_pEditScene->Finalize();
		m_pEditScene.reset();
	}

    m_pEditScene = std::make_unique<EditorScene>();
    m_pEditScene->Initialize(m_engineContext);

	// Load the scene data from file
    bool result = SceneLoader::LoadScene(filePath, m_pEditScene.get());

    if (result) DBG("EditorApp: Loaded scene from %s", filePath.c_str());
    else        DBG("EditorApp: Failed to load scene from %s", filePath.c_str());

	// Set the viewport size based on the current scene color render target
    ApplySceneRenderTargetSizeToScene(*m_pEditScene);
}

// Hot reload: rebuild GameCode.dll and reload it without restarting the Editor.
//
// Order of operations matters for safety:
// 1. Save a snapshot of the current scene to disk (to preserve all actors and components).
// 2. Buld the GameCode.dll which is staged to a temporary file (GameCode.staged.dll).
// 3. Destroy the current runtime state (editor editing related, registered components factory, loaded DLL).
// 4. Promote the staged DLL to become the new active GameCode.dll (backing up the previous DLL).
// 5. Reconstruct the scene from the saved snapshot with the new GameCode.dll loaded.
// 6. Remove the previous GameCode DLL backup after a successful hot reload.
// If staging fails, the current scene and GameCode remain untouched. If committing
// the staged DLL fails, restore the previous DLL and reconstruct the scene snapshot.
void EditorApp::ReloadGameCode(bool reconfigure)
{
	// 1. Save a snapshot of the current scene to disk (to preserve all actors and components).
    if (!SaveHotReloadSnapshot())
    {
        DBG("EditorApp: Hot reload aborted because " "the scene snapshot could not be saved.");
        return;
    }

	// 2. Build the GameCode.dll which is staged to a temporary file (GameCode.staged.dll).
    if (!BuildStagedGameCode(reconfigure))
    {
        DBG("EditorApp: Staging build failed. " "The current scene and GameCode remain unchanged.");
        return;
    }

	// 3. Destroy the current runtime state (editor editing related, registered components factory, loaded DLL).
    if (!DestroyCurrentRuntimeState())
    {
        DBG("EditorApp: Failed to destroy the current " "GameCode runtime state.");

		// Restore the previous GameCode DLL and the scene from the snapshot

        if (!RestorePreviousGameCode())
        {
            DBG("EditorApp: Failed to recover the previous " "GameCode DLL after runtime destruction failure.");
            return;
        }

        if (!RestoreHotReloadSnapshot())
        {
            DBG("EditorApp: Failed to restore the scene after " "runtime destruction failure.");
        }

        return;
    }

	// 4. Promote the staged DLL to become the new active GameCode.dll (backing up the previous DLL).
    if (!PromoteStagedGameCode())
    {
        DBG("EditorApp: Failed to promote the staged " "GameCode DLL. Restoring the previous DLL.");

        // Restore the previous GameCode DLL and the scene from the snapshot

        if (!RestorePreviousGameCode())
        {
            DBG("EditorApp: Failed to restore the previous " "GameCode DLL. The snapshot remains on disk.");
            return;
        }

        if (!RestoreHotReloadSnapshot())
        {
            DBG("EditorApp: Failed to restore the scene " "using the previous GameCode DLL.");
        }

        return;
    }

	// 5. Reconstruct the scene from the saved snapshot with the new GameCode.dll loaded.
    if (!RestoreHotReloadSnapshot())
    {
        DBG("EditorApp: Scene restoration failed with " "the new GameCode DLL. Rolling back.");

        // Restore the previous GameCode DLL and the scene from the snapshot

        if (!RollbackGameCode())
        {
            DBG("EditorApp: Failed to roll back GameCode. " "The snapshot remains on disk.");
            return;
        }

        if (!RestoreHotReloadSnapshot())
        {
            DBG("EditorApp: Scene restoration also failed " "after restoring the previous GameCode DLL.");
        }

        return;
    }

	// 6. Remove the previous GameCode DLL backup after a successful hot reload.
    RemovePreviousGameCodeBackup();

    DBG("EditorApp: GameCode hot reload completed.");
}

bool EditorApp::SaveHotReloadSnapshot()
{
    if (!m_pEditScene)
    {// In case of empty scene
        DBG("EditorApp: ReloadGameCode - no active scene, aborting.");
        return false;
    }

    if (!SceneWriter::SaveScene(kHotReloadScenePath, m_pEditScene.get()))
    {// In case of save failure
        DBG("EditorApp: ReloadGameCode - failed to save scene snapshot, aborting reload.");
        return false;
    }

    return true;
}

bool EditorApp::BuildStagedGameCode(bool reconfigure)
{
	if (reconfigure)
	{
		if (!ProjectBuilder::Reconfigure())
		{
			DBG("EditorApp: ReloadGameCode - reconfigure failed, aborting reload.");
			return false;
		}
	}

	return ProjectBuilder::BuildGameCodeForHotReload("Debug");
}

bool EditorApp::DestroyCurrentRuntimeState()
{
    StopAllEditTransactions();

    m_hierarchyPanel.ClearSelection();
    m_commandHistory.Clear();

    if (m_pPlayScene)
    {
        m_pPlayScene->Finalize();
        m_pPlayScene.reset();
    }

    if (m_pEditScene)
    {
        m_pEditScene->Finalize();
        m_pEditScene.reset();
    }

    ComponentRegistry::Get().UnregisterAllGameComponents();

    if (!m_hGameCodeDll)
    {
        DBG("EditorApp::DestroyCurrentRuntimeState: " "No GameCode DLL is loaded.");
        return true;
    }

    if (!FreeLibrary(m_hGameCodeDll))
    {
        DBG( "EditorApp::DestroyCurrentRuntimeState: " "FreeLibrary failed with error %lu.", GetLastError());
        return false;
    }

    m_hGameCodeDll = nullptr;
    return true;
}

bool EditorApp::PromoteStagedGameCode()
{
    // Replace the active GameCode.dll with the newly built one, while keeping a backup of the previous version.
    namespace fs = std::filesystem;

    const fs::path activeDll = PathManager::Resolve("build/bin/Debug/GameCode.dll");
    const fs::path stagedDll = PathManager::Resolve("build/bin/Debug/GameCode.staged.dll");
    const fs::path previousDll = PathManager::Resolve("build/bin/Debug/GameCode.previous.dll");

	// Check if the newly built staged DLL exists before attempting to promote it
    if (!fs::exists(stagedDll))
    {
        DBG("EditorApp::PromoteStagedGameCode: " "The staged DLL does not exist.");
        return false;
    }

    std::error_code error;

	// Remove the previous backup DLL before backup the current active DLL
    fs::remove(previousDll, error);

    if (error)
    {
        DBG("Failed to back up GameCode.dll.");
        return false;
    }

    error.clear();

	// Back up the current active DLL to GameCode.previous.dll
    if (fs::exists(activeDll))
    {
        fs::rename(activeDll, previousDll, error);

        if (error)
        {
            DBG("EditorApp::PromoteStagedGameCode: " "Failed to back up the active DLL: %s", error.message().c_str());
            return false;
        }
    }

	// Rename the staged DLL to become the new active DLL
    error.clear();
    fs::rename(stagedDll, activeDll, error);

    if (error)
    {
        DBG("EditorApp::PromoteStagedGameCode: " "Failed to promote the staged DLL: %s", error.message().c_str());
        return false;
    }

	// Load the newly promoted GameCode.dll into the editor process
    m_hGameCodeDll = LoadLibraryA(activeDll.string().c_str());

	// Check if the LoadLibrary call succeeded
    if (!m_hGameCodeDll)
    {
        DBG("EditorApp::PromoteStagedGameCode: " "LoadLibrary failed with error %lu.", GetLastError());
        return false;
    }

    DBG("EditorApp::PromoteStagedGameCode: " "The staged DLL was promoted successfully.");
    return true;
}

bool EditorApp::RestorePreviousGameCode()
{
    namespace fs = std::filesystem;

    const fs::path activePath = PathManager::Resolve(kActiveGameCodePath);
    const fs::path previousPath = PathManager::Resolve(kPreviousGameCodePath);

	// Remove registration after failing to load the previous DLL
    ComponentRegistry::Get().UnregisterAllGameComponents();

	// Free the current failed GameCode.dll if it is still loaded
    if (m_hGameCodeDll)
    {
        if (!FreeLibrary(m_hGameCodeDll))
        {
            DBG("EditorApp::RestorePreviousGameCode: " "Failed to unload the current DLL.");
            return false;
        }

        m_hGameCodeDll = nullptr;
    }

    std::error_code error;

	// Replace the active DLL with the previous DLL if it exists
    if (fs::exists(previousPath))
    {
        fs::remove(activePath, error);

        if (error)
        {
            DBG("EditorApp::RestorePreviousGameCode: " "Failed to remove the failed active DLL: %s", error.message().c_str());
            return false;
        }

        error.clear();
        fs::rename(previousPath, activePath, error);

        if (error)
        {
            DBG("EditorApp::RestorePreviousGameCode: " "Failed to restore the previous DLL: %s", error.message().c_str());
            return false;
        }
    }

    if (!fs::exists(activePath))
    {
        DBG("EditorApp::RestorePreviousGameCode: " "No active or previous DLL exists.");
        return false;
    }

	// Rollback to the previous GameCode.dll
    m_hGameCodeDll = LoadLibraryA(activePath.string().c_str());

    if (!m_hGameCodeDll)
    {
        DBG("EditorApp::RestorePreviousGameCode: " "LoadLibrary failed with error %lu.", GetLastError());
        return false;
    }

    return true;
}

bool EditorApp::RollbackGameCode()
{
    ComponentRegistry::Get().UnregisterAllGameComponents();

    if (m_hGameCodeDll)
    {
        if (!FreeLibrary(m_hGameCodeDll))
        {
            DBG("EditorApp::RollbackGameCode: " "Failed to unload the new GameCode DLL.");
            return false;
        }

        m_hGameCodeDll = nullptr;
    }

    return RestorePreviousGameCode();
}

bool EditorApp::RestoreHotReloadSnapshot()
{
    auto restoredScene = std::make_unique<EditorScene>();

	restoredScene->Initialize(m_engineContext); // Initialize scene before loading

	// Load the stored scene snapshot from the temporary file
    if (!SceneLoader::LoadScene(kHotReloadScenePath, restoredScene.get()))
    {
        DBG("EditorApp::RestoreHotReloadSnapshot: " "Failed to deserialize the snapshot.");

        restoredScene->Finalize();
        return false;
    }

    m_pEditScene = std::move(restoredScene);

	// Apply the current viewport size to the restored scene to ensure proper camera settings
    ApplySceneRenderTargetSizeToScene(*m_pEditScene);

    return true;

}

void EditorApp::RemovePreviousGameCodeBackup()
{
    namespace fs = std::filesystem;

    const fs::path previousPath = PathManager::Resolve(kPreviousGameCodePath);

    std::error_code error;
    fs::remove(previousPath, error);

    if (error)
    {
        DBG("EditorApp::RemovePreviousGameCodeBackup: " "Failed to remove the previous DLL: %s", error.message().c_str());
    }
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

void EditorApp::EnterPlayMode()
{
	if (m_editorMode == EditorMode::Play)
	{
		DBG("EditorApp: Already in Play mode.");
		return;
	}

	if (!m_pEditScene)
	{
		DBG("EditorApp: No active scene to enter Play mode.");
		return;
	}

	// Stop any ongoing editing transactions before switching to Play Mode
	StopAllEditTransactions();

	// Clone editor scene to create a separate runtime scene for Play Mode
	std::unique_ptr<SceneBase> playScene = SceneCloner::Clone(m_pEditScene.get(), m_engineContext);

	if (!playScene)
	{
		DBG("EditorApp: Failed to clone scene for Play mode.");
		return;
	}

	// Move ownership of the cloned scene to m_pPlayScene
    m_pPlayScene = std::move(playScene);

	// Resize the scene render targets to the fixed resolution for Play Mode
    if (!m_pEngine->ResizeSceneRenderTargets(PLAY_VIEWPORT_WIDTH, PLAY_VIEWPORT_HEIGHT))
    {
		DBG("EditorApp: Failed to resize scene render targets for Play mode.");
		m_pPlayScene->Finalize();
		m_pPlayScene.reset();
		return;
    }

    // Apply the render target size to viewport-dependent elements
    // in the Play scene, such as Screen-Space UI layout.
    ApplySceneRenderTargetSizeToScene(*m_pPlayScene);

	// Switch to Play Mode
	m_editorMode = EditorMode::Play;
}

void EditorApp::ExitPlayMode()
{
	if (m_pPlayScene == nullptr)
	{
		DBG("EditorApp: No active Play scene to exit from.");
        m_editorMode = EditorMode::Edit;
        m_hierarchyPanel.ClearSelection();
		return;
	}

	if (m_editorMode != EditorMode::Play)
	{
		DBG("EditorApp: Not in Play mode.");
		return;
	}

	// Stop any ongoing editing transactions before switching back to Edit Mode
	StopAllEditTransactions();

	// Get selected actor GUID before destroying the play scene
	// to check if the selected actor is still valid in the edit scene after exiting Play Mode
    Guid selectedActorId = m_hierarchyPanel.GetSelectedActorGuid();

    // Destroy the play scene
	m_pPlayScene->Finalize();
	m_pPlayScene.reset();

	// Switch back to Edit Mode
	m_editorMode = EditorMode::Edit;

	// Apply the current viewport size to the edit scene to ensure proper camera settings
    const ImVec2 viewportSize = m_sceneViewPanel.GetViewportSize();

    const UINT width = static_cast<UINT>(viewportSize.x);
    const UINT height = static_cast<UINT>(viewportSize.y);

    if (width > 0 && height > 0)
    {
        if (!m_pEngine->ResizeSceneRenderTargets(width, height))
        {
            DBG("EditorApp: Failed to resize scene render targets.");
        }

        GpuTexture* sceneColor = m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::SceneColor);

        if (sceneColor)
        {
            const UINT actualWidth = sceneColor->GetWidth();
            const UINT actualHeight = sceneColor->GetHeight();

            CameraLens lens = m_pEditorCamera->GetCameraLens();
            lens.width = static_cast<float>(actualWidth);
            lens.height = static_cast<float>(actualHeight);
            m_pEditorCamera->SetCameraLens(lens);

            if (m_pEditScene)
            {
                ApplySceneRenderTargetSizeToScene(*m_pEditScene);
            }
        }
    }

	// Check if the selected Actor is still valid in the edit scene, if not clear the selection
	if (selectedActorId.IsValid())
	{
		Actor* selectedActor = m_pEditScene ? m_pEditScene->ResolveActor(selectedActorId) : nullptr;

        if (selectedActor)
        {
            m_hierarchyPanel.SelectActor(selectedActorId);
        }
        else
		{// In case of the selected actor is no longer valid in the edit scene or the edit scene is null
            m_hierarchyPanel.ClearSelection();
        }
	}
}

void EditorApp::ApplyPendingModeTransition()
{
    const EditorModeTransition transition = m_pendingModeTransition;
    m_pendingModeTransition = EditorModeTransition::None;

    switch (transition)
    {
    case EditorModeTransition::EnterPlay:
        EnterPlayMode();
        break;

    case EditorModeTransition::ExitPlay:
        ExitPlayMode();
        break;

    case EditorModeTransition::None:
        break;
    }
}

void EditorApp::PrepareInstance()
{
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
    m_pEngine->InitCore(m_window.GetHandle(), WINDOW_WIDTH, WINDOW_HEIGHT);

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
    m_pEngine->EndFrame();

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

    namespace fs = std::filesystem;
    const fs::path iniPath = PathManager::ResolveW("Saved/Editor/imgui.ini");
    std::error_code directoryError;
    fs::create_directories(iniPath.parent_path(), directoryError);
    if (directoryError)
    {
        DBG("EditorApp: Failed to create ImGui settings directory: %s", directoryError.message().c_str());
    }

    m_imguiIniPath = StringEncoding::WideToUtf8(iniPath.wstring());
    if (m_imguiIniPath.empty())
    {
        DBG("EditorApp: Failed to convert the ImGui settings path to UTF-8.");
    }
    m_shouldBuildDefaultDockLayout = !fs::exists(iniPath);
    io.IniFilename = m_imguiIniPath.empty() ? nullptr : m_imguiIniPath.c_str();

	//---------------------------------
	// Load custom font for the editor
	//---------------------------------
    EditorFontConfig fontConfig;
    fontConfig.filePath = "C:/Windows/Fonts/Meiryo.ttc";
    fontConfig.sizePixels = 16.0f;
    fontConfig.fontIndex = 2;

    EditorTheme::LoadFont(io, fontConfig);

	//-------------------------------------------
	// Initialize ImGui for Win32 and DirectX 12
	//-------------------------------------------
    ImGui_ImplWin32_Init(m_window.GetHandle());

    auto descriptorHeapAllocator = m_pEngine->GetDescriptorHeapAllocator();
    uint32_t imguiIndex = descriptorHeapAllocator->AllocateCbvSrvUav();

    ImGui_ImplDX12_Init(
        m_pEngine->GetDevice(),
        SwapChain::BufferCount,
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

	// Apply mode transitions before command recording begins. Toolbar callbacks
	// run while ImGui is being rendered and must not resize GPU resources directly.
    ApplyPendingModeTransition();

	// Handle any pending resize requests for the scene view panel
    ApplySceneViewResizeRequest();

    // Update the editor camera actor
    // (call PreUpdate, Update, and LateUpdate in sequence to update camera component)
    m_pEditorCameraActor->PreUpdate(deltaTime);
    m_pEditorCameraActor->Update(deltaTime);
    m_pEditorCameraActor->LateUpdate(deltaTime);

    // Flush the transform of the editor camera actor
    m_pEditorCameraActor->FlushTransform();

	SceneBase* activeScene = GetActiveScene();

    // Advance gameplay only in Play Mode.
    // Edit Mode synchronizes editor-visible scene state without running gameplay callbacks.
    if (activeScene)
    {
        if (m_editorMode == EditorMode::Play)
        {
            activeScene->PreUpdate(deltaTime);
            activeScene->Update(deltaTime);
            activeScene->LateUpdate(deltaTime);
        }
        else
        {
            activeScene->EditorUpdate(deltaTime);
        }
    }

	// Get the current camera information based on the editor mode (Play or Edit)
    const CameraInfo* currentCamera = nullptr;

	if (m_editorMode == EditorMode::Play)
	{// In case of Play Mode
		// Use camera from the active scene's CameraSystem in Play Mode
		CameraSystem* cameraSystem = activeScene ? activeScene->GetCameraSystem() : nullptr;
		currentCamera = cameraSystem ? cameraSystem->GetCameraInfo() : nullptr;
	}
	else if (m_editorMode == EditorMode::Edit)
	{// In case of Edit Mode
		// Use the editor camera in Edit Mode
		currentCamera = m_pEditorCamera ? &m_pEditorCamera->GetCameraInfo() : nullptr;
	}

	// Update the renderer with the current camera information for rendering
    if (currentCamera)
    {
		m_pRenderer->Update(m_pEngine->GetCurrentBufferIndex(), *currentCamera);
    }
}

void EditorApp::Render()
{
    m_pEngine->BeginFrame();
    m_pRenderer->BeginFrame(m_pEngine->GetCommandList());

    m_pTextureManager->UploadPendingTextures(m_pEngine->GetCommandList());

	SceneBase* activeScene = GetActiveScene();

	// Build CameraInfo based on the current viewport mode and size
    GpuTexture* sceneColor = m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::SceneColor);

    if (sceneColor)
    {
        if (m_editorMode == EditorMode::Play)
        {
            RenderPlayViewport(activeScene, sceneColor);
        }
        else
        {
            RenderEditViewport(activeScene, sceneColor);
        }
    }

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

    m_pEngine->EndFrame();
}

void EditorApp::RenderEditViewport(SceneBase* activeScene, GpuTexture* sceneColor)
{
    const EditorViewportMode viewportMode = m_sceneViewPanel.GetViewMode();
    const bool isSceneView = viewportMode == EditorViewportMode::Scene;

    // Set up the render view policy based on the current viewport mode
    RenderViewPolicy viewPolicy{};

    // CanvasEditContext is a persistent edit scope. Initialize it from the
    // current selection only when no valid Canvas is already open.
    Canvas* editingCanvas = nullptr;

    // Canvas View mode requires a valid Canvas
    if (!isSceneView && activeScene)
    {
        // Resolve the currently editing Canvas from the CanvasEditContext
        editingCanvas = m_canvasEditContext.ResolveCanvas(*activeScene);

        // First selection of a Canvas in the hierarchy panel opens the CanvasEditContext for editing.
        if (!editingCanvas)
        {
            Actor* selectedActor = m_hierarchyPanel.GetSelectedActor(activeScene);

            if (m_canvasEditContext.OpenFromActor(selectedActor))
            {
                editingCanvas = m_canvasEditContext.ResolveCanvas(*activeScene);
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

    const CameraInfo viewportCameraInfo = BuildViewportCameraInfo(sceneColor->GetWidth(), sceneColor->GetHeight());

    if (activeScene)
    {
        activeScene->OnRender(m_engineContext, &viewportCameraInfo, viewPolicy);
    }

    const RenderSpace targetRenderSpace = isSceneView
        ? RenderSpace::World : RenderSpace::Screen;

    // Build render data for the selected object in the scene view
    // (for outline rendering)
    BuildSelectionRenderData(targetRenderSpace, viewportCameraInfo, editingCanvas);

    // Scene View only requires shadow rendering
    if (isSceneView)
    {
		RenderShadowPass();
		RenderWorldPass();
    }
    else
    {
        RenderPassTarget canvasViewTarget
		{
			RenderPassTargetType::ColorDepth,
			static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneColor),
			static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneDepth)
		};

		m_pEngine->BeginPass(canvasViewTarget);

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

		m_pEngine->EndPass(canvasViewTarget);
    }

	RenderSelectionPass();
}

void EditorApp::RenderPlayViewport(SceneBase* activeScene, GpuTexture* sceneColor)
{
    if (!activeScene) return;

    CameraSystem* cameraSystem = activeScene->GetCameraSystem();

    if (!cameraSystem || !cameraSystem->GetMainCamera())
    {
        DBG("EditorApp::RenderPlayViewport: " "Play scene has no main camera.");
        return;
    }

    // Don't override camera settings. Use the camera in the runtime scene
	// No policy for rendering space filter or canvas view root for just playing the game
    activeScene->OnRender(m_engineContext);

    RenderShadowPass();
    RenderWorldPass();

    RenderPassTarget sceneTarget
    {
        RenderPassTargetType::ColorDepth,
        static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneColor),
        static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneDepth),
        false,
        false
    };

    m_pEngine->BeginPass(sceneTarget);

    m_pRenderer->RenderScreenSpace(
        m_pEngine->GetCommandList(),
        sceneColor->GetWidth(),
        sceneColor->GetHeight(),
        RenderTargetFormat::HDR
    );

    m_pEngine->EndPass(sceneTarget);
}

void EditorApp::RenderShadowPass()
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

void EditorApp::RenderWorldPass()
{
	RenderPassTarget sceneTarget
	{
		RenderPassTargetType::ColorDepth,
		static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneColor),
		static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneDepth)
	};

    m_pEngine->BeginPass(sceneTarget);

    GpuTexture* shadowMap = m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::ShadowMap);

    if (shadowMap)
    {
        m_pRenderer->RenderScene(m_pEngine->GetCommandList(), shadowMap->GetSrvIndex());
    }

	m_pEngine->EndPass(sceneTarget);
}

void EditorApp::RenderSelectionPass()
{
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
    m_pRenderer->RenderSelectionMask(m_pEngine->GetCommandList(), m_selectionRenderData);
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

}


void EditorApp::RenderImGui()
{
    RenderMenuBar();
    RenderMainDockSpace();

    RenderHierarchyPanel();
    RenderInspectorPanel();
    RenderSceneViewPanel();
	RenderToolbar();
    RenderScriptsPanel();

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pEngine->GetCommandList());
}

void EditorApp::RenderMainDockSpace()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiID dockSpaceId = ImHashStr("EditorDockSpace");

    // Build only for a new workspace or an explicit reset. Rebuilding every
    // frame would overwrite layout changes made by the user.
    if (m_shouldBuildDefaultDockLayout)
    {
        BuildDefaultDockLayout(dockSpaceId);
        m_shouldBuildDefaultDockLayout = false;
    }

    ImGui::DockSpaceOverViewport(dockSpaceId, viewport);
}

void EditorApp::BuildDefaultDockLayout(unsigned int dockSpaceId)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::DockBuilderRemoveNode(dockSpaceId);
    ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockSpaceId, viewport->WorkSize);

    ImGuiID centerNode = dockSpaceId;
    ImGuiID leftNode = 0;
    ImGuiID rightNode = 0;
    ImGuiID toolbarNode = 0;
    ImGuiID scriptsNode = 0;

    ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Left, 0.20f, &leftNode, &centerNode);
    ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Right, 0.30f, &rightNode, &centerNode);
    ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Up, 0.08f, &toolbarNode, &centerNode);
    ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Down, 0.25f, &scriptsNode, &centerNode);

    ImGui::DockBuilderDockWindow("Hierarchy", leftNode);
    ImGui::DockBuilderDockWindow("Inspector", rightNode);
    ImGui::DockBuilderDockWindow("Toolbar", toolbarNode);
    ImGui::DockBuilderDockWindow("Scripts", scriptsNode);
    ImGui::DockBuilderDockWindow("Scene", centerNode);
    ImGui::DockBuilderFinish(dockSpaceId);
}

void EditorApp::RenderHierarchyPanel()
{
    HierarchyPanel::Callbacks callbacks;

	callbacks.onRenameActor = [this](const Guid& targetActorGuid, const std::string& newName) -> bool
		{
			if (!m_pEditScene || m_editorMode != EditorMode::Edit) return false;

			Actor* actor = m_pEditScene->ResolveActor(targetActorGuid);
			if (!actor) return false;

			const std::string oldName = actor->GetName();
			const bool succeeded = m_commandHistory.Execute(
				std::make_unique<RenameActorCommand>(
					m_pEditScene.get(),
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
            if (!m_pEditScene || m_editorMode != EditorMode::Edit) return;

            Actor::InitDesc desc;
            desc.name = name;

            const bool succeeded = m_commandHistory.Execute(
                std::make_unique<CreateActorCommand>(m_pEditScene.get(), desc, parentGuid)
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
			if (!m_pEditScene || m_editorMode != EditorMode::Edit || !actorGuid.IsValid()) return false;

			Actor* actor = m_pEditScene->ResolveActor(actorGuid);
			if (!actor || actor->IsDestroyed()) return false;

            const std::string actorName = actor->GetName();

            const bool succeeded = m_commandHistory.Execute(
                std::make_unique<DeleteActorCommand>(
                    m_pEditScene.get(),
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
			if (!m_pEditScene || m_editorMode != EditorMode::Edit || !actorGuid.IsValid()) return false;

			Actor* actor = m_pEditScene->ResolveActor(actorGuid);
			if (!actor || actor->IsDestroyed()) return false;

			Actor* newParent = newParentGuid.IsValid()
				? m_pEditScene->ResolveActor(newParentGuid) : nullptr;

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
                        m_pEditScene.get(),
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

    SceneBase* activeScene = GetActiveScene();

	callbacks.onOpenCanvas = [this, activeScene](const Guid& actorGuid)
		{
			if (!activeScene || !actorGuid.IsValid()) return;

			Actor* actor = activeScene->ResolveActor(actorGuid);
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

	callbacks.canEdit = m_editorMode == EditorMode::Edit && m_pEditScene != nullptr;

    m_hierarchyPanel.Render(activeScene, callbacks);
}

void EditorApp::RenderInspectorPanel()
{
    InspectorContext context;
	context.assetManager = m_pAssetManager.get();

    InspectorState inspectorState = InspectorState::ReadOnly;
    if (m_pEditScene && m_editorMode == EditorMode::Edit) inspectorState = InspectorState::Editable;
    context.state = inspectorState;

    if (inspectorState == InspectorState::Editable)
    {
        // Transform editing callbacks
        context.onTransformEditBegin = [this](const Guid& actorGuid, const Transform3D& before) { BeginTransformEdit(actorGuid, before); };
        context.onTransformEditEnd = [this](const Guid& actorGuid, const Transform3D& after) { EndTransformEdit(actorGuid, after); };
        context.onCancelTransformEdit = [this]() { CancelTransformEdit(); };

        // RectTransform editing callbacks
        context.onRectTransformEditBegin = [this](const Guid& actorGuid, const RectTransformEditState& before) { BeginRectTransformEdit(actorGuid, before); };
        context.onRectTransformEditEnd = [this](const Guid& actorGuid, const RectTransformEditState& after) { EndRectTransformEdit(actorGuid, after); };
        context.onCancelRectTransformEdit = [this]() { CancelRectTransformEdit(); };
    }

    InspectorPanel::Callbacks callbacks;

	// Callback for adding a component to an actor.
    callbacks.onAddComponent =
        [this, inspectorState](const Guid& actorGuid, const std::string& componentName)
        {
            if (!m_pEditScene || inspectorState != InspectorState::Editable) return false;

            return m_commandHistory.Execute(
                std::make_unique<AddComponentCommand>(
                    m_pEditScene.get(),
                    actorGuid,
                    componentName
                )
            );
        };

	// Callback for removing a component from an actor.
    callbacks.onRemoveComponent =
        [this, inspectorState](
            const Guid& actorGuid,
            const std::string& componentName,
            std::size_t occurrenceIndex)
        {
            if (!m_pEditScene || inspectorState != InspectorState::Editable) return false;

            return m_commandHistory.Execute(
                std::make_unique<RemoveComponentCommand>(
                    m_pEditScene.get(),
                    actorGuid,
                    componentName,
                    occurrenceIndex
                )
            );
        };

	SceneBase* activeScene = GetActiveScene();
	m_inspectorPanel.Render(m_hierarchyPanel.GetSelectedActor(activeScene), context, callbacks);
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
    ViewportOverlayData overlayData;

    if (m_editorMode == EditorMode::Edit)
    {
        overlayData = BuildViewportOverlayData(sceneColor->GetWidth(), sceneColor->GetHeight());
    }

	// Render the scene view panel with the scene color render target
    m_sceneViewPanel.Render(
        gpuHandle,
        sceneColor->GetWidth(),
        sceneColor->GetHeight(),
		overlayData
    );

    if (m_editorMode == EditorMode::Edit)
    {
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

        SceneBase* activeScene = GetActiveScene();

        if (m_sceneViewPanel.ConsumeCanvasOpenRequest(canvasActorGuid))
        {
            Actor* canvasActor = activeScene
                ? activeScene->ResolveActor(canvasActorGuid) : nullptr;

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
        if (!activeScene || !m_pEditorCamera) return;

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
            editingCanvas = m_canvasEditContext.ResolveCanvas(*activeScene);
        }

        // Get the picked Actor information
        const std::optional<ScenePickHit> hit = ScenePicker::Pick(*activeScene, pickCameraInfo, pickUV, targetRenderSpace, editingCanvas);

        if (hit)
        {
            m_hierarchyPanel.SelectActor(hit->actorGuid);
        }
        else
        {
            m_hierarchyPanel.ClearSelection();
        }
    }
}

void EditorApp::RenderMenuBar()
{
    MenuBar::Callbacks callbacks;

    callbacks.onNewScene = [this]()
        {
            if (m_editorMode != EditorMode::Edit) return;
            NewScene();
        };

    callbacks.onOpenScene = [this]()
        {
            if (m_editorMode != EditorMode::Edit) return;
            LoadScene(kDefaultScenePath);
        };

    callbacks.onSaveScene = [this]()
        {
            if (m_editorMode != EditorMode::Edit) return;

            if (m_pEditScene && !SceneWriter::SaveScene(kDefaultScenePath, m_pEditScene.get()))
            {
                DBG("EditorApp: Save failed.");
            }
        };

    callbacks.onUndo = [this]()
        {
            if (m_editorMode != EditorMode::Edit) return;

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
            if (m_editorMode != EditorMode::Edit) return;

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

    callbacks.onResetLayout = [this]()
        {
            m_shouldBuildDefaultDockLayout = true;
        };

    callbacks.onBuildGame = [this]()
        {
            if (m_editorMode != EditorMode::Edit) return;

            ProjectBuilder::ReconfigureAndBuild("101Game", "Debug");
        };

    callbacks.onReloadGameCode = [this](bool reconfigure)
        {
            if (m_editorMode != EditorMode::Edit) return;

            ReloadGameCode(reconfigure);
        };

    callbacks.onCreateScript = [this](const std::string& name, bool isBehavior)
        {
            if (m_editorMode != EditorMode::Edit) return;

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

    callbacks.canUndo = m_editorMode == EditorMode::Edit && m_pEditScene != nullptr && m_commandHistory.CanUndo();
    callbacks.canRedo = m_editorMode == EditorMode::Edit && m_pEditScene != nullptr && m_commandHistory.CanRedo();
    callbacks.canEditScene = m_editorMode == EditorMode::Edit && m_pEditScene != nullptr;
	callbacks.canModifyScripts = m_editorMode == EditorMode::Edit && m_pEditScene != nullptr;
    callbacks.canBuild = m_editorMode == EditorMode::Edit && m_pEditScene != nullptr;

    m_menuBar.Render(callbacks);
}

void EditorApp::RenderToolbar()
{
    Toolbar::Callbacks callbacks;

    callbacks.onPlay = [this]()
        {
            m_pendingModeTransition = EditorModeTransition::EnterPlay;
        };

    callbacks.onStop = [this]()
        {
            m_pendingModeTransition = EditorModeTransition::ExitPlay;
        };

    callbacks.canPlay = m_editorMode == EditorMode::Edit && m_pEditScene != nullptr;
    callbacks.canStop = m_editorMode == EditorMode::Play && m_pPlayScene != nullptr;

    m_toolbar.Render(callbacks);
}

void EditorApp::RenderScriptsPanel()
{
    ScriptsPanel::Callbacks callbacks;

    callbacks.onDelete = [this](const std::string& name)
        {
			if (m_editorMode != EditorMode::Edit) return;
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

	callbacks.canDelete = m_editorMode == EditorMode::Edit && m_pEditScene != nullptr;

    m_scriptsPanel.Render(callbacks, m_pEditScene.get());
}

void EditorApp::ShutdownImGui()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

SceneBase* EditorApp::GetActiveScene() const
{
	if (m_editorMode == EditorMode::Edit && m_pEditScene)
	{
		return m_pEditScene.get();
	}

	if (m_editorMode == EditorMode::Play && m_pPlayScene)
	{
		return m_pPlayScene.get();
	}

	return nullptr;
}


void EditorApp::ApplySceneViewResizeRequest()
{
	UINT width = 0, height = 0;

	// Check if the scene view panel has requested a resize of the render target
    if (!m_sceneViewPanel.ConsumeResizeRequest(width, height))
    {
        return;
    }

	// Fix the reslolution in the Play Mode to the current scene render target size (no resizing allowed)
    if (m_editorMode == EditorMode::Play)
    {
        return;
    }
    
	// Resize the scene render targets (color and depth) to the new width and height
    if (!m_pEngine->ResizeSceneRenderTargets(width, height)) 
    {
        return;
    }

	// Update the editor camera's lens parameters
	CameraLens lens = m_pEditorCamera->GetCameraLens();
	lens.width = static_cast<float>(width);
	lens.height = static_cast<float>(height);

	m_pEditorCamera->SetCameraLens(lens);

    // Apply the viewport size to the scene and invalidate
    // all layout elements affected by the size change
    if (m_pEditScene)
    {
        m_pEditScene->SetViewportSize(width, height);
    }
}

void EditorApp::ApplySceneRenderTargetSizeToScene(SceneBase& scene)
{
    if (!m_pEngine) 
    {
        return;
    }

    GpuTexture* sceneColor = m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::SceneColor);

	if (!sceneColor) 
    {
        return;
    }

	scene.SetViewportSize(sceneColor->GetWidth(), sceneColor->GetHeight());
}

void EditorApp::BeginTransformEdit(const Guid& actorGuid, const Transform3D& before)
{
	if (m_editorMode != EditorMode::Edit) return;

	// Validate the actorGuid
    if (!m_pEditScene || !actorGuid.IsValid())
    {
		CancelTransformEdit();
        return;
    }

	Actor* actor = m_pEditScene->ResolveActor(actorGuid);

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
    if (m_editorMode != EditorMode::Edit) return;

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
            m_pEditScene.get(),
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

    if (!m_pEditScene || !actorGuid.IsValid())
    {
        m_transformEditTransaction.reset();
        return;
    }

	Actor* actor = m_pEditScene->ResolveActor(actorGuid);

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
    if (m_editorMode != EditorMode::Edit) return;

	if (!m_pEditScene || !actorGuid.IsValid())
	{
		CancelRectTransformEdit();
		return;
	}

	Actor* actor = m_pEditScene->ResolveActor(actorGuid);

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
    if (m_editorMode != EditorMode::Edit) return;

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
		    m_pEditScene.get(),
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

    if (!m_pEditScene || !actorGuid.IsValid())
    {
        m_rectTransformEditTransaction.reset();
        return;
    }

	Actor* actor = m_pEditScene->ResolveActor(actorGuid);

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

	SceneBase* activeScene = GetActiveScene();

    if (!activeScene) return;

	// Get the currently selected actor from the hierarchy panel and validate it
	Actor* selectedActor = m_hierarchyPanel.GetSelectedActor(activeScene);

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

	SceneBase* activeScene = GetActiveScene();

	if (!activeScene || viewportWidth == 0 || viewportHeight == 0)
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
		Canvas* editingCanvas = m_canvasEditContext.ResolveCanvas(*activeScene);
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
		Actor* selectedActor = m_hierarchyPanel.GetSelectedActor(activeScene);
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
	Actor* selectedActor = m_hierarchyPanel.GetSelectedActor(activeScene);
	Canvas* selectedCanvas = CanvasEditContext::FindClosestCanvas(selectedActor);

	const Vector3 localCorners[4]
	{
		{ -0.5f, -0.5f, 0.0f },
		{ -0.5f,  0.5f, 0.0f },
		{  0.5f,  0.5f, 0.0f },
		{  0.5f, -0.5f, 0.0f }
	};

	// Iterate through all actors in the scene to find world-space canvases and build overlay rects for them
	for (Actor* actor : activeScene->GetAllActors())
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
	SceneBase* activeScene = GetActiveScene();

    if (activeScene)
    {
        Canvas* canvas = m_canvasEditContext.ResolveCanvas(*activeScene);
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

void EditorApp::StopAllEditTransactions()
{
	CancelTransformEdit();
	CancelRectTransformEdit();
}
