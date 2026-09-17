#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <mmsystem.h>
#include <tchar.h>
#include <shellapi.h>
#include "Core/EditorApp.h"
#include "Engine/Input/InputManager.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"
#include "Core/EditorScene.h"
#include "Document/SceneEditorDocument.h"
#include "Document/ActorImprintEditorDocument.h"
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
#include "Command/ChangeActorTagCommand.h"
#include "Tag/TagManagementWorkflow.h"
#include "Command/CreateActorCommand.h"
#include "Command/DeleteActorCommand.h"
#include "Command/DeleteActorImprintInstanceCommand.h"
#include "Command/ReparentActorCommand.h"
#include "Command/AddComponentCommand.h"
#include "Command/RemoveComponentCommand.h"
#include "Command/ComponentPropertyEditCommand.h"
#include "Command/TransformEditCommand.h"
#include "Command/InstantiateActorImprintCommand.h"
#include "ActorImprint/ActorImprintAssetWorkflow.h"
#include "ActorImprint/ActorImprintEditingContext.h"
#include "UI/EditorTheme.h"
#include "Scene/ScenePicker.h"
#include "Scene/SceneCloner.h"
#include "Scene/SceneAssetWorkflow.h"
#include "Engine/Project/ProjectSettings.h"

#pragma comment(lib, "winmm.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static const int WINDOW_WIDTH  = 1280;
static const int WINDOW_HEIGHT = 720;

namespace
{
#ifndef ENGINE_BUILD_CONFIGURATION
#error ENGINE_BUILD_CONFIGURATION must identify the Editor build configuration.
#endif

	constexpr const char* kBuildConfiguration = ENGINE_BUILD_CONFIGURATION;
    constexpr const char* kHotReloadScenePath = "build/_hotreload_temp.scene";

	std::string GetGameCodeBuildPath(const char* fileName)
	{
		return "build/bin/" + std::string(kBuildConfiguration) + "/" + fileName;
	}

    // Temporary Scene View navigation tuning. These values intentionally live
    // outside EditorViewCamera so they can later move to editor preferences.
    constexpr float kLookRadiansPerPixel = 0.003f;
    constexpr float kOrbitRadiansPerPixel = 0.003f;
    constexpr float kPanDistanceScalePerPixel = 0.002f;
    constexpr float kDollyDistanceFractionPerStep = 0.12f;
    constexpr float kFastMoveMultiplier = 4.0f;
    constexpr float kMaximumPitchRadians = PI_DIV_2 - 0.01f;
    constexpr float kMinimumPivotDistance = 0.1f;
    constexpr float kFocusPadding = 1.2f;
	constexpr float kAssetRefreshIntervalSeconds = 0.5f;

}

bool EditorApp::Initialize()
{
	m_documentWorkflow.SetSaveCallback(
		[this](EditorDocumentId id) { return SaveEditorDocument(id); });
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
    windowDesc.messageCallback = [this](
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        LRESULT& outResult)
        {
			if (message == WM_CLOSE)
			{
				if (!HandleWindowCloseRequest())
				{
					return false;
				}

				outResult = 0;
				return true;
			}

            if (ImGui::GetCurrentContext() &&
                ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
            {
                outResult = TRUE;
                return true;
            }

			InputManager::GetInstance().ProcessWindowMessage(message, wParam, lParam);

            switch (message)
            {
            case WM_KEYDOWN:
                if (wParam == VK_ESCAPE)
                {
					SendMessage(hwnd, WM_CLOSE, 0, 0);
				}

				break;
            }

            return false;
        };

    if (!m_window.Initialize(windowDesc))
    {
        DBG("EditorApp: Failed to initialize the main window.");
        return false;
    }

	PrepareInstance(); // Prepare instance

	if (!InitInstance())
	{
		return false;
	}

	InitImGui();                    // Initialize ImGui

	std::string settingsError;

	if (!ProjectSettings::Load(PathManager::Resolve("project.101"), m_projectSettings, &settingsError))
	{
		m_operationDiagnostic = "Project settings could not be loaded: " + settingsError;
		m_openOperationDiagnosticPopup = true;
		NewScene();
	}
	else
	{
		for (const std::string& tag : m_projectSettings.GetUserTags())
		{
			if (TagRegistry::Get().RegisterUserTag(tag, nullptr, &settingsError))
			{
				continue;
			}

			m_operationDiagnostic = "Project Tags could not be registered: " + settingsError;
			m_openOperationDiagnosticPopup = true;
			break;
		}

		if (!settingsError.empty())
		{
			NewScene();
		}
		else if (!m_projectSettings.GetEditorStartupSceneGuid().IsValid() ||
				 !LoadScene(m_projectSettings.GetEditorStartupSceneGuid()))
		{
			if (m_projectSettings.GetEditorStartupSceneGuid().IsValid())
			{
				m_operationDiagnostic = "The configured Editor Startup Scene could not be loaded.";
				m_openOperationDiagnosticPopup = true;
			}

			NewScene();
		}
	}

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

                // ImGui's NewFrame is called before Update so debug ImGui
                // windows can be drawn from within Update().
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
    if (m_pPlaySceneManager)
    {
        m_pPlayScene = nullptr;
        m_pPlaySceneManager->Finalize();
        m_pPlaySceneManager.reset();
    }

	m_documentManager.Clear();

    m_pActorImprintSystem.reset();
	m_engineContext.pActorImprintSystem = nullptr;
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
	StopAllEditTransactions();
	auto scene = CreateDefaultScene();

	auto* document = static_cast<SceneEditorDocument*>(m_documentManager.FindFirst(EditorDocumentType::Scene));

	if (document)
	{
		document->ReplaceScene(std::move(scene), {}, true);
		m_documentManager.ActivateDocument(document);
	}
	else
	{
		auto newDocument = std::make_unique<SceneEditorDocument>(
			std::move(scene), std::string{}, WINDOW_WIDTH, WINDOW_HEIGHT);
		newDocument->MarkDirty();
		m_documentManager.AddDocument(std::move(newDocument));
	}

    DBG("EditorApp: New scene created.");
}

// Load a scene from a file path
void EditorApp::LoadScene(const std::string& filePath)
{
	SceneLoadResult load = SceneLoader::LoadCandidate(filePath, m_engineContext);

	if (!load)
	{
		DBG("EditorApp: Failed to load scene from %s at '%s': %s",
			filePath.c_str(), load.error.path.c_str(), load.error.message.c_str());
		return;
	}

	// Complete viewport-dependent state on the private candidate before the
	// editor publishes it or invalidates any state owned by the current Scene.
	ApplySceneRenderTargetSizeToScene(*load.scene);

    // Cancel ongoing editing transactions
    StopAllEditTransactions();

	auto* document = static_cast<SceneEditorDocument*>(m_documentManager.FindFirst(EditorDocumentType::Scene));

	if (document)
	{
		document->ReplaceScene(std::move(load.scene), filePath);
		m_documentManager.ActivateDocument(document);
	}
	else
	{
		m_documentManager.AddDocument(std::make_unique<SceneEditorDocument>(
			std::move(load.scene), filePath, WINDOW_WIDTH, WINDOW_HEIGHT));
	}

	DBG("EditorApp: Loaded scene from %s", filePath.c_str());
}

bool EditorApp::LoadScene(const Guid& sceneAssetGuid)
{
	if (!m_pAssetManager)
	{
		return false;
	}

	const AssetEntry* entry = m_pAssetManager->GetAssetEntry(sceneAssetGuid);

	if (!entry || entry->type != AssetType::Scene)
	{
		return false;
	}

	const std::string path = m_pAssetManager->GetAssetPath(sceneAssetGuid);
	SceneLoadResult load = SceneLoader::LoadCandidate(path, m_engineContext);

	if (!load)
	{
		return false;
	}

	ApplySceneRenderTargetSizeToScene(*load.scene);
	StopAllEditTransactions();
	auto* document = static_cast<SceneEditorDocument*>(m_documentManager.FindFirst(EditorDocumentType::Scene));

	if (document)
	{
		document->ReplaceScene(std::move(load.scene), path, false, sceneAssetGuid);
		m_documentManager.ActivateDocument(document);
	}
	else
	{
		m_documentManager.AddDocument(std::make_unique<SceneEditorDocument>(
			std::move(load.scene), path, WINDOW_WIDTH, WINDOW_HEIGHT, sceneAssetGuid));
	}

	m_sceneAssetPanel.Select(sceneAssetGuid);
	return true;
}

bool EditorApp::RequestCreateScene(std::string_view name)
{
	m_pendingSceneAction = PendingSceneAction::Create;
	m_pendingSceneName.assign(name);
	m_pendingSceneGuid = {};
	auto* document = m_documentManager.FindFirst(EditorDocumentType::Scene);

	if (!document)
	{
		return ExecutePendingSceneAction();
	}

	if (document->IsDirty())
	{
		m_documentDecisionDiagnostic.clear();
		m_openDocumentDecisionPopup = true;
		return true;
	}

	return ExecutePendingSceneAction();
}

bool EditorApp::RequestOpenScene(const Guid& sceneAssetGuid)
{
	if (!sceneAssetGuid.IsValid())
	{
		return false;
	}

	auto* current = static_cast<SceneEditorDocument*>(m_documentManager.FindFirst(EditorDocumentType::Scene));

	if (current && current->GetSourceAssetGuid() == sceneAssetGuid)
	{
		return m_documentManager.ActivateDocument(current);
	}

	m_pendingSceneAction = PendingSceneAction::Open;
	m_pendingSceneName.clear();
	m_pendingSceneGuid = sceneAssetGuid;

	if (!current)
	{
		return ExecutePendingSceneAction();
	}

	if (current->IsDirty())
	{
		m_documentDecisionDiagnostic.clear();
		m_openDocumentDecisionPopup = true;
		return true;
	}

	return ExecutePendingSceneAction();
}

bool EditorApp::ExecutePendingSceneAction()
{
	const PendingSceneAction action = m_pendingSceneAction;
	const std::string name = std::move(m_pendingSceneName);
	const Guid guid = m_pendingSceneGuid;
	m_pendingSceneAction = PendingSceneAction::None;
	m_pendingSceneName.clear();
	m_pendingSceneGuid = {};

	if (action == PendingSceneAction::Open)
	{
		return LoadScene(guid);
	}

	if (action != PendingSceneAction::Create)
	{
		return false;
	}

	auto scene = CreateDefaultScene();
	Guid createdGuid;
	std::string path;
	SceneAssetWorkflowError error;

	if (!SceneAssetWorkflow::Create(name, *scene, *m_pAssetManager, createdGuid, path, &error))
	{
		m_sceneAssetPanel.SetDiagnostic(error.message);
		return false;
	}

	auto* document = static_cast<SceneEditorDocument*>(m_documentManager.FindFirst(EditorDocumentType::Scene));

	if (document)
	{
		document->ReplaceScene(std::move(scene), path, false, createdGuid);
		m_documentManager.ActivateDocument(document);
	}
	else
	{
		m_documentManager.AddDocument(
			std::make_unique<SceneEditorDocument>(std::move(scene), path, WINDOW_WIDTH, WINDOW_HEIGHT, createdGuid));
	}

	m_sceneAssetPanel.Select(createdGuid);
	m_sceneAssetPanel.SetDiagnostic("Scene asset created.");
	return true;
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
	DBG("EditorApp: Starting %s GameCode hot reload%s.",
		kBuildConfiguration, reconfigure ? " with reconfigure" : "");

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
	SceneBase* editScene = GetEditScene();

	if (!editScene)
    {// In case of empty scene
        DBG("EditorApp: ReloadGameCode - no active scene, aborting.");
        return false;
    }

    if (!SceneWriter::SaveScene(kHotReloadScenePath, editScene))
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

	return ProjectBuilder::BuildGameCodeForHotReload(kBuildConfiguration);
}

bool EditorApp::DestroyCurrentRuntimeState()
{
    StopAllEditTransactions();

    if (m_pPlaySceneManager)
    {
        m_pPlayScene = nullptr;
        m_pPlaySceneManager->Finalize();
        m_pPlaySceneManager.reset();
    }

	m_documentManager.ReleaseWorkingScenesForRuntimeReload();

    // Resolved type information must not survive unloading its Component DLL.
    m_pActorImprintSystem->Clear();
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

    const fs::path activeDll = PathManager::Resolve(GetGameCodeBuildPath("GameCode.dll"));
    const fs::path stagedDll = PathManager::Resolve(GetGameCodeBuildPath("GameCode.staged.dll"));
    const fs::path previousDll = PathManager::Resolve(GetGameCodeBuildPath("GameCode.previous.dll"));

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

    const fs::path activePath = PathManager::Resolve(GetGameCodeBuildPath("GameCode.dll"));
    const fs::path previousPath = PathManager::Resolve(GetGameCodeBuildPath("GameCode.previous.dll"));

	// Remove registration after failing to load the previous DLL
    m_pActorImprintSystem->Clear();
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
			DBG("EditorApp::RestorePreviousGameCode: "
				"Failed to remove the failed active DLL: %s",
				error.message().c_str());
			return false;
		}

		error.clear();
        fs::rename(previousPath, activePath, error);

        if (error)
		{
			DBG("EditorApp::RestorePreviousGameCode: "
				"Failed to restore the previous DLL: %s",
				error.message().c_str());
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
    m_pActorImprintSystem->Clear();
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
	SceneLoadOptions options
	{
		// Ignore unknown properties of component to accept changes
		// in GameCode component definitions after hot reload.
	   .unknownComponentPropertyPolicy = UnknownPropertyPolicy::Ignore,
	};

	// Load scene temporary to check if the loading is successful before publishing it to the editor document.
	SceneLoadResult load = SceneLoader::LoadCandidate(
		kHotReloadScenePath,
		m_engineContext,
		options);

	if (!load)
    {// In case of failure
		DBG("EditorApp::RestoreHotReloadSnapshot: Failed at '%s': %s",
			load.error.path.c_str(), load.error.message.c_str());
		return false;
    }

	// Complete caller-owned viewport state before publishing the restored candidate.
	ApplySceneRenderTargetSizeToScene(*load.scene);

	auto* document = static_cast<SceneEditorDocument*>(m_documentManager.FindFirst(EditorDocumentType::Scene));

	if (!document)
	{
		return false;
	}

	const bool wasDirty = document->IsDirty();
	document->ReplaceScene(std::move(load.scene), document->GetFilePath(), wasDirty);
	m_documentManager.ActivateDocument(document);

    return true;

}

void EditorApp::RemovePreviousGameCodeBackup()
{
    namespace fs = std::filesystem;

    const fs::path previousPath = PathManager::Resolve(GetGameCodeBuildPath("GameCode.previous.dll"));

    std::error_code error;
    fs::remove(previousPath, error);

    if (error)
	{
		DBG("EditorApp::RemovePreviousGameCodeBackup: "
			"Failed to remove the previous DLL: %s",
			error.message().c_str());
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

	IEditorDocument* document = GetActiveEditDocument();
	SceneBase* editScene = document ? document->GetWorkingScene() : nullptr;

	if (!document || !document->CanEnterPlay() || !editScene)
	{
		DBG("EditorApp: No active scene to enter Play mode.");
		return;
	}

	// Stop any ongoing editing transactions before switching to Play Mode
	StopAllEditTransactions();

	// Clone editor scene to create a separate runtime scene for Play Mode
	std::unique_ptr<SceneBase> playScene = SceneCloner::Clone(editScene, m_engineContext);

	if (!playScene)
	{
		DBG("EditorApp: Failed to clone scene for Play mode.");
		return;
	}

	// Resize the scene render targets to the fixed resolution for Play Mode
    if (!m_pEngine->ResizeSceneRenderTargets(PLAY_VIEWPORT_WIDTH, PLAY_VIEWPORT_HEIGHT))
    {
		DBG("EditorApp: Failed to resize scene render targets for Play mode.");
		playScene->Finalize();
		return;
    }

    // Apply the render target size to viewport-dependent elements
    // in the Play scene, such as Screen-Space UI layout.
    ApplySceneRenderTargetSizeToScene(*playScene);

	// Keep the unsaved Edit Scene clone for the first Play Scene. SceneManager
	// owns it so Behavior::ChangeScene can load subsequent Scene assets.
	m_pPlaySceneManager = std::make_unique<SceneManager>();
	m_pPlaySceneManager->SetViewportSize(PLAY_VIEWPORT_WIDTH, PLAY_VIEWPORT_HEIGHT);
	m_pPlaySceneManager->RegisterScene("EditorPlay", std::move(playScene));
	m_pPlaySceneManager->SetInitialScene("EditorPlay");
	m_pPlaySceneManager->Initialize(m_engineContext);
	m_pPlayScene = m_pPlaySceneManager->GetCurrentScene();

	if (!m_pPlayScene)
	{
		m_pPlaySceneManager->Finalize();
		m_pPlaySceneManager.reset();
		return;
	}

	// Switch to Play Mode
	m_editorMode = EditorMode::Play;
}

void EditorApp::ExitPlayMode()
{
	if (m_pPlayScene == nullptr)
	{
		DBG("EditorApp: No active Play scene to exit from.");
		m_editorMode = EditorMode::Edit;

		if (EditorSelection* selection = GetActiveSelection())
		{
			selection->Clear();
		}

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
	EditorSelection* selection = GetActiveSelection();
	const Guid selectedActorId = selection ? selection->GetSelectedActorGuid() : Guid{};

    // Destroy the Play Scene before returning to the Edit Scene.
	m_pPlayScene = nullptr;
	m_pPlaySceneManager->Finalize();
	m_pPlaySceneManager.reset();

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

			EditorViewportContext* viewport = GetActiveViewportContext();

			if (!viewport)
			{
				return;
			}

			Camera& editorCamera = viewport->GetSceneCamera().GetCamera();
            CameraLens lens = editorCamera.GetCameraLens();
            lens.width = static_cast<float>(actualWidth);
            lens.height = static_cast<float>(actualHeight);
            editorCamera.SetCameraLens(lens);

			SceneBase* editScene = GetEditScene();

			if (editScene)
            {
				ApplySceneRenderTargetSizeToScene(*editScene);
            }
        }
    }

	// Check if the selected Actor is still valid in the edit scene, if not clear the selection
	if (selectedActorId.IsValid())
	{
		SceneBase* editScene = GetEditScene();
		Actor* selectedActor = editScene ? editScene->ResolveActor(selectedActorId) : nullptr;

        if (selectedActor)
        {
			if (selection)
			{
				selection->SelectActor(selectedActorId);
			}
		}
        else
		{ // In case of the selected actor is no longer valid in the edit scene or the edit scene is null
			StopAllEditTransactions();

			if (selection)
			{
				selection->Clear();
			}
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
    m_pActorImprintSystem = std::make_unique<ActorImprintSystem>(*m_pAssetManager);

    m_engineContext = {
        m_pRenderer.get(),
        m_pTextureManager.get(),
        m_pMeshManager.get(),
        m_pAssetManager.get(),
        m_pActorImprintSystem.get()
    };

}

std::unique_ptr<SceneBase> EditorApp::CreateDefaultScene()
{
	auto scene = std::make_unique<EditorScene>();
	scene->Initialize(m_engineContext);
	Actor::InitDesc cameraDesc;
	cameraDesc.name = "DefaultCamera";
	cameraDesc.tag = ActorTags::MainCamera;
	auto cameraActorOwned = ActorFactory::CreateActor(ActorType::Camera, cameraDesc);
	cameraActorOwned->GetComponentByClass<Transform>()->SetParams(
		Transform::ParamDesc{.localPosition = {0, 0, -5}});
	auto* camera = cameraActorOwned->GetComponentByClass<Camera>();
	camera->SetParams(Camera::ParamDesc{.window_width = WINDOW_WIDTH, .window_height = WINDOW_HEIGHT});
	scene->AddRootActor(std::move(cameraActorOwned));
	scene->GetCameraSystem()->SetMainCamera(camera);
	ApplySceneRenderTargetSizeToScene(*scene);
	return scene;
}

bool EditorApp::InitInstance()
{
    m_pEngine->InitCore(m_window.GetHandle(), WINDOW_WIDTH, WINDOW_HEIGHT);

    auto pDevice = m_pEngine->GetDevice();

	m_pTextureManager->Initialize(pDevice, m_pEngine->GetDescriptorHeapAllocator());
	m_pMeshManager->Initialize(pDevice, m_pTextureManager.get());

	if (!m_pAssetManager->Initialize(
			PathManager::Resolve("asset"),
			m_pTextureManager.get(),
			m_pMeshManager.get()))
	{
		return false;
	}

	m_pEngine->InitBindings(m_pTextureManager.get());
	m_pRenderer->Initialize(
		pDevice, m_pEngine->GetDescriptorHeapAllocator(), m_pTextureManager.get(), m_pMeshManager.get());

	m_pEngine->BeginFrame();
    m_pEngine->EndFrame();

    InputManager::GetInstance().Initialize();
	return true;
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

void EditorApp::Update(float deltaTime)
{
    InputManager::GetInstance().Update();

	// Apply mode transitions before command recording begins. Toolbar callbacks
	// run while ImGui is being rendered and must not resize GPU resources directly.
    ApplyPendingModeTransition();

	// Handle any pending resize requests for the scene view panel
    ApplySceneViewResizeRequest();

	SceneBase* activeScene = GetActiveScene();

    // Advance gameplay only in Play Mode.
    // Edit Mode synchronizes editor-visible scene state without running gameplay callbacks.
    if (activeScene)
    {
        if (m_editorMode == EditorMode::Play)
        {
			m_pPlaySceneManager->PreUpdate(deltaTime);
			m_pPlaySceneManager->Update(deltaTime);
			m_pPlayScene = m_pPlaySceneManager->GetCurrentScene();
			m_pPlaySceneManager->LateUpdate(deltaTime);
        }
        else
        {
            activeScene->EditorUpdate(deltaTime);
        }
    }

	// ActorImprint state remains fixed for the whole Play session. The elapsed
	// time is retained so Edit mode observes filesystem changes promptly on exit.
	RefreshAssetCatalog(deltaTime);

	if (m_editorMode == EditorMode::Edit)
	{
		ProcessActorImprintAssetChanges();
	}

	activeScene = GetActiveScene();

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
		EditorViewportContext* viewport = GetActiveViewportContext();
		currentCamera = viewport
			? &viewport->GetSceneCamera().GetCamera().GetCameraInfo() : nullptr;
	}

	// Update the renderer with the current camera information for rendering
    if (currentCamera)
    {
		m_pRenderer->Update(m_pEngine->GetCurrentBufferIndex(), *currentCamera);
    }
}

void EditorApp::RefreshAssetCatalog(float deltaTime)
{
	if (!m_pAssetManager)
	{
		return;
	}

	if (std::isfinite(deltaTime) && deltaTime > 0.0f)
	{
		m_assetRefreshElapsedSeconds += deltaTime;
	}

	if (m_editorMode != EditorMode::Edit)
	{
		return;
	}

	if (m_assetRefreshElapsedSeconds < kAssetRefreshIntervalSeconds)
	{
		return;
	}

	m_assetRefreshElapsedSeconds = 0.0f;

	AssetCatalogError error;

	if (!m_pAssetManager->Refresh(&error))
	{
		DBG("EditorApp: Asset catalog refresh failed at '%s': %s",
			error.path.c_str(), error.message.c_str());
	}
}

bool EditorApp::HasActiveEditTransaction() const
{
	return m_inspectorPanel.HasActiveEdit() ||
		m_transformEditTransaction.has_value() ||
		m_rectTransformEditTransaction.has_value();
}

bool EditorApp::ProcessActorImprintAssetChanges(const Guid* requiredAsset)
{
	if (!m_pAssetManager || !m_pActorImprintSystem ||
		m_editorMode != EditorMode::Edit || HasActiveEditTransaction())
	{
		if (requiredAsset)
		{
			DBG("ActorImprint reload cannot run while the Editor is busy.");
		}

		return requiredAsset == nullptr;
	}

	auto changes = m_pAssetManager->TakePendingChanges();
	bool observedRequiredAsset = requiredAsset == nullptr;
	bool requiredAssetSucceeded = requiredAsset == nullptr;

	for (const AssetChange& change : changes)
	{
		const bool isRequired = requiredAsset && change.guid == *requiredAsset;

		if (isRequired)
		{
			observedRequiredAsset = true;
		}

		const ActorImprintHandle loaded = m_pActorImprintSystem->FindHandle(change.guid);

		if (change.type != AssetType::ActorImprint && loaded.IsNull())
		{
			if (isRequired)
			{
				DBG("Saved ActorImprint change was not reloadable.");
			}

			continue;
		}

		ActorImprintReloadResult result = m_documentManager.ReloadActorImprint(*m_pActorImprintSystem, change);

		if (result.status == ActorImprintReloadStatus::Failed)
		{
			continue;
		}

		if (result.status == ActorImprintReloadStatus::Missing)
		{
			DBG("ActorImprint %s is missing; its loaded definition and Instances were retained.",
				change.guid.ToString().c_str());
			continue;
		}

		if (isRequired && (result.status == ActorImprintReloadStatus::Reloaded ||
							  result.status == ActorImprintReloadStatus::NoChange))
		{
			requiredAssetSucceeded = true;
		}

		if (result.status != ActorImprintReloadStatus::Reloaded)
		{
			continue;
		}

		m_selectionRenderData.Clear();
		result.FinalizeRetiredScenes();
	}

	if (requiredAsset && !observedRequiredAsset)
	{
		DBG("Saved ActorImprint did not publish a reload notification.");
	}

	return observedRequiredAsset && requiredAssetSucceeded;
}

bool EditorApp::SaveEditorDocument(EditorDocumentId id)
{
	IEditorDocument* document = m_documentManager.FindDocument(id);

	if (!document)
	{
		return false;
	}

	StopAllEditTransactions();

	if (document->GetType() != EditorDocumentType::ActorImprint)
	{
		return document->Save();
	}

	auto* imprintDocument = static_cast<ActorImprintEditorDocument*>(document);

	if (!imprintDocument->PrepareSave())
	{
		return false;
	}

	const Guid assetGuid = imprintDocument->GetSourceAssetGuid();

	if (!ProcessActorImprintAssetChanges(&assetGuid))
	{
		imprintDocument->GetEditingContext()->RollbackPendingSave();
		document->MarkDirty();
		return false;
	}

	imprintDocument->CommitPreparedSave();
	return true;
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
	EditorViewportContext* viewport = GetActiveViewportContext();
	EditorSelection* selection = GetActiveSelection();

	if (!viewport)
	{
		return;
	}

	const EditorViewportMode viewportMode = viewport->GetViewMode();
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
		CanvasEditContext& canvasContext = viewport->GetCanvasEditContext();
		editingCanvas = canvasContext.ResolveCanvas(*activeScene);

        // First selection of a Canvas in the hierarchy panel opens the CanvasEditContext for editing.
        if (!editingCanvas)
        {
			Actor* selectedActor = selection ? selection->ResolveActor(activeScene) : nullptr;

			DBG("EditorApp::RenderEditViewport: CanvasEditContext has no valid "
				"Canvas. Attempting to open from selected Actor.");

			if (canvasContext.OpenFromActor(selectedActor))
            {
				editingCanvas = canvasContext.ResolveCanvas(*activeScene);
            }
			else
			{
				DBG("EditorApp::RenderEditViewport: Failed to open CanvasEditContext from selected Actor.");
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
		BuildColliderDebugRenderData(activeScene);
		RenderColliderDebugPass();
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
	if (!activeScene)
	{
		return;
	}

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

void EditorApp::RenderColliderDebugPass()
{
	if (!m_showColliders || m_colliderDebugRenderData.meshs.empty())
	{
		return;
	}

	RenderPassTarget sceneTarget
	{
		RenderPassTargetType::ColorDepth,
		static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneColor),
		static_cast<uint32_t>(Engine::BuiltinRenderTarget::SceneDepth),
		false,
		false
	};

	m_pEngine->BeginPass(sceneTarget);
	m_pRenderer->RenderColliderDebug(
		m_pEngine->GetCommandList(), m_colliderDebugRenderData);
	m_pEngine->EndPass(sceneTarget);
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
	RenderActorImprintsPanel();
	RenderSceneAssetPanel();
	RenderTagManagerPanel();
	RenderDocumentDecisionModal();
	RenderOperationDiagnosticModal();

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
    ImGui::DockBuilderDockWindow("Actor Imprints", scriptsNode);
	ImGui::DockBuilderDockWindow("Scenes", scriptsNode);
    ImGui::DockBuilderDockWindow("Scene", centerNode);
    ImGui::DockBuilderFinish(dockSpaceId);
}

void EditorApp::RenderHierarchyPanel()
{
	IEditorDocument* document = GetActiveEditDocument();
	SceneBase* editScene = document ? document->GetWorkingScene() : nullptr;
	EditorSelection* selection = document ? &document->GetSelection() : nullptr;
	EditorViewportContext* viewport = document ? &document->GetViewportContext() : nullptr;
    HierarchyPanel::Callbacks callbacks;
	callbacks.onSelectionChanging = [this]()
	{
		StopAllEditTransactions();
	};

	callbacks.onRenameActor = [this, document, editScene](
								  const Guid& targetActorGuid, const std::string& newName) -> bool
	{
		if (!document || !editScene || m_editorMode != EditorMode::Edit)
		{
			return false;
		}

		Actor* actor = editScene->ResolveActor(targetActorGuid);

		if (!actor)
		{
			return false;
		}

		const std::string oldName = actor->GetName();
		const bool succeeded =
			document->ExecuteCommand(std::make_unique<RenameActorCommand>(editScene, targetActorGuid, newName));

		if (succeeded)
		{
			DBG(
				"EditorApp: Renamed Actor '%s' to '%s' through command history.",
				oldName.c_str(),
				newName.c_str());
		}
		else
		{
			DBG("EditorApp: Failed to rename Actor '%s' to '%s'.", oldName.c_str(), newName.c_str());
		}

		return succeeded;
	};

	callbacks.onCreateActor = [this, document, editScene](const std::string& name, const Guid& parentGuid) -> bool
	{
		if (!document || !editScene || m_editorMode != EditorMode::Edit)
		{
			return false;
		}

		Actor::InitDesc desc;
		desc.name = name;

		const bool succeeded = document->ExecuteCommand(
			std::make_unique<CreateActorCommand>(editScene, desc, parentGuid));

		if (succeeded)
		{
			DBG(
				"EditorApp: Created new actor '%s' through command history.",
				name.c_str());
		}
		else
		{
			DBG("EditorApp: Failed to create actor '%s'.", name.c_str());
		}

		if (succeeded)
		{
			m_hierarchyPanel.SetDiagnostic({});
		}

		return succeeded;
	};

	callbacks.onDeleteActor =
		[this, document, editScene](const Guid& actorGuid) -> bool
	{
		if (!document || !editScene || m_editorMode != EditorMode::Edit || !actorGuid.IsValid())
		{
			return false;
		}

		Actor* actor = editScene->ResolveActor(actorGuid);

		if (!actor || actor->IsDestroyed())
		{
			return false;
		}

		const std::string actorName = actor->GetName();

		const auto* member = editScene->GetImprintInstances().FindMember(actor->GetHandle());
		std::unique_ptr<IEditorCommand> command;

		if (member && m_pActorImprintSystem)
		{
			command = std::make_unique<DeleteActorImprintInstanceCommand>(
				*editScene, *m_pActorImprintSystem, actorGuid);
		}
		else
		{
			command = std::make_unique<DeleteActorCommand>(editScene, actorGuid);
		}

		const bool succeeded = document->ExecuteCommand(std::move(command));

		if (succeeded)
		{
			DBG("EditorApp: Deleted Actor '%s' through command history.", actorName.c_str());
		}
		else
		{
			DBG("EditorApp: Failed to delete Actor '%s'.", actorName.c_str());
		}

		if (succeeded)
		{
			m_hierarchyPanel.SetDiagnostic({});
		}

		return succeeded;
	};

	callbacks.onReparentActor =
		[this, document, editScene](const Guid& actorGuid, const Guid& newParentGuid) -> bool
	{
		if (!document || !editScene || m_editorMode != EditorMode::Edit || !actorGuid.IsValid())
		{
			return false;
		}

		Actor* actor = editScene->ResolveActor(actorGuid);

		if (!actor || actor->IsDestroyed())
		{
			return false;
		}

		Actor* newParent = newParentGuid.IsValid()
							   ? editScene->ResolveActor(newParentGuid)
							   : nullptr;

		if (newParentGuid.IsValid() &&
			(!newParent || newParent->IsDestroyed()))
		{
			return false;
		}

		// Get name for debug logging
		const std::string actorName = actor->GetName();
		const std::string parentName = newParent
										   ? newParent->GetName()
										   : "Scene Root";

		// Execute the reparenting command through the command history
		const bool succeeded =
			document->ExecuteCommand(
				std::make_unique<ReparentActorCommand>(
					editScene,
					actorGuid,
					newParentGuid));

		if (succeeded)
		{
			DBG("EditorApp: Reparented Actor '%s' to '%s' through command history.", actorName.c_str(),
				parentName.c_str());
		}
		else
		{
			DBG("EditorApp: Failed to reparent Actor '%s' to '%s'.", actorName.c_str(), parentName.c_str());
		}

		if (succeeded)
		{
			m_hierarchyPanel.SetDiagnostic({});
		}

		return succeeded;
	};

	callbacks.onInstantiateActorImprint =
		[this, document, editScene, selection](const Guid& assetGuid, const Guid& parentGuid) -> bool
	{
		if (!document || document->GetType() != EditorDocumentType::Scene || !editScene ||
			!selection || !m_pActorImprintSystem || m_editorMode != EditorMode::Edit)
		{
			return false;
		}

		auto command =
			std::make_unique<InstantiateActorImprintCommand>(*editScene, *m_pActorImprintSystem, assetGuid, parentGuid);

		if (!command->Execute())
		{
			return false;
		}

		const Guid rootActorGuid = command->GetRootActorGuid();

		if (!document->RecordExecutedCommand(std::move(command)))
		{
			return false;
		}

		StopAllEditTransactions();
		selection->SelectActor(rootActorGuid);

		DBG("EditorApp: Instantiated ActorImprint %s through command history.",
			assetGuid.ToString().c_str());
		return true;
	};

	SceneBase* activeScene = GetActiveScene();

	callbacks.onOpenCanvas = [viewport, activeScene](const Guid& actorGuid)
	{
		if (!viewport || !activeScene || !actorGuid.IsValid())
		{
			return;
		}

		Actor* actor = activeScene->ResolveActor(actorGuid);

		if (!actor || actor->IsDestroyed() || !actor->GetComponentByClass<Canvas>())
		{
			return;
		}

		// Open the CanvasActor in the Canvas View by setting the Actor in the context
		if (viewport->GetCanvasEditContext().OpenFromActor(actor))
		{
			// Set the scene view panel to Canvas mode when a canvas is opened
			viewport->SetViewMode(EditorViewportMode::Canvas);
		}
	};

	callbacks.canEdit = m_editorMode == EditorMode::Edit && editScene != nullptr;

	if (selection)
	{
		m_hierarchyPanel.Render(activeScene, *selection, callbacks);
	}
}

void EditorApp::RenderInspectorPanel()
{
	IEditorDocument* document = GetActiveEditDocument();
	SceneBase* editScene = document ? document->GetWorkingScene() : nullptr;
	EditorSelection* selection = document ? &document->GetSelection() : nullptr;
    InspectorContext context;
	context.assetManager = m_pAssetManager.get();
	context.scene = GetActiveScene();

	InspectorState inspectorState = InspectorState::ReadOnly;

	if (editScene && m_editorMode == EditorMode::Edit)
	{
		inspectorState = InspectorState::Editable;
	}

	context.state = inspectorState;

    InspectorPanel::Callbacks callbacks;
	callbacks.onChangeActorTag =
		[this, document, editScene, inspectorState](const Guid& actorGuid, TagId newTag)
	{
		if (!document || !editScene || inspectorState != InspectorState::Editable)
		{
			return false;
		}

		const bool succeeded =
			document->ExecuteCommand(std::make_unique<ChangeActorTagCommand>(editScene, actorGuid, newTag));

		if (!succeeded)
		{
			m_operationDiagnostic = "The Actor tag edit could not be committed to the active Document.";
			m_openOperationDiagnosticPopup = true;
		}

		return succeeded;
	};

	// Callback for adding a component to an actor.
	callbacks.onAddComponent =
		[this, document, editScene, inspectorState](const Guid& actorGuid, const std::string& componentName)
	{
		if (!document || !editScene || inspectorState != InspectorState::Editable)
		{
			return false;
		}

		const bool succeeded =
			document->ExecuteCommand(std::make_unique<AddComponentCommand>(editScene, actorGuid, componentName));

		if (!succeeded)
		{
			DBG("The structural operation failed.");
		}

		return succeeded;
	};

	// Callback for removing a component from an actor.
	callbacks.onRemoveComponent =
		[this, document, editScene, inspectorState](
			const Guid& actorGuid,
			const std::string& componentName,
			std::size_t occurrenceIndex)
	{
		if (!document || !editScene || inspectorState != InspectorState::Editable)
		{
			return false;
		}

		m_inspectorPanel.CancelActiveEdit();

		const bool succeeded = document->ExecuteCommand(
			std::make_unique<RemoveComponentCommand>(editScene, actorGuid, componentName, occurrenceIndex));

		if (!succeeded)
		{
			DBG("The structural operation failed.");
		}

		return succeeded;
	};

	callbacks.onEditProperty =
		[this, document, editScene, inspectorState](
			const ComponentPropertyIdentity& identity,
			const PropertyValue& before,
			const PropertyValue& after)
	{
		if (!document || !editScene || inspectorState != InspectorState::Editable)
		{
			return false;
		}

		const bool succeeded = document->RecordExecutedCommand(
			std::make_unique<ComponentPropertyEditCommand>(editScene, identity, before, after));

		if (!succeeded)
		{
			m_operationDiagnostic = "The property edit could not be committed to the active Document.";
			m_openOperationDiagnosticPopup = true;
		}

		return succeeded;
	};

	SceneBase* activeScene = GetActiveScene();
	m_inspectorPanel.Render(selection ? selection->ResolveActor(activeScene) : nullptr,
		context, callbacks);
}

void EditorApp::RenderSceneViewPanel()
{
	EditorViewportContext* viewport = GetActiveViewportContext();
	EditorSelection* selection = GetActiveSelection();
	const std::vector<EditorDocumentInfo> documents = m_documentManager.GetDocuments();
	std::optional<EditorDocumentId> activateDocumentRequest;
	std::optional<EditorDocumentId> closeDocumentRequest;
	DocumentTabBar::Callbacks documentCallbacks;
	documentCallbacks.canInteract = m_editorMode == EditorMode::Edit &&
		!m_documentWorkflow.IsExitPending();
	documentCallbacks.onActivate = [&activateDocumentRequest](EditorDocumentId id)
	{
		activateDocumentRequest = id;
	};
	documentCallbacks.onClose = [&closeDocumentRequest](EditorDocumentId id)
	{
		closeDocumentRequest = id;
	};
	// Get the scene color render target from the engine
	GpuTexture* sceneColor =  m_pEngine->GetBuiltinRenderTarget(Engine::BuiltinRenderTarget::SceneColor);

	if (!sceneColor)
	{
		return;
	}

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
		viewport,
		overlayData,
		documents,
		documentCallbacks
	);

	// Document changes are committed only after SceneViewPanel is finished with
	// pointers from the previously active Document. A tab interaction also ends
	// viewport input handling for this frame.
	if (closeDocumentRequest)
	{
		StopAllEditTransactions();
		const EditorDocumentWorkflowResult result = m_documentWorkflow.RequestClose(*closeDocumentRequest);

		if (result == EditorDocumentWorkflowResult::ConfirmationRequired)
		{
			m_documentDecisionDiagnostic.clear();
			m_openDocumentDecisionPopup = true;
		}
		else if (result == EditorDocumentWorkflowResult::Closed)
		{
			m_selectionRenderData.Clear();
		}

		return;
	}

	if (activateDocumentRequest)
	{
		StopAllEditTransactions();

		if (m_documentWorkflow.Activate(*activateDocumentRequest))
		{
			m_selectionRenderData.Clear();
		}

		return;
	}

	if (m_editorMode == EditorMode::Edit && viewport)
    {
        SceneBase* activeScene = GetActiveScene();

		SceneNavigationInput sceneNavigationInput;

		if (m_sceneViewPanel.ConsumeSceneNavigationInput(sceneNavigationInput))
        {
            ApplySceneNavigationInput(
                sceneNavigationInput,
                m_timeManager.GetDeltaTime());
        }

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
            Actor* canvasActor = activeScene
                ? activeScene->ResolveActor(canvasActorGuid) : nullptr;

            if (canvasActor && !canvasActor->IsDestroyed() && canvasActor->GetComponentByClass<Canvas>())
            {
				viewport->GetCanvasEditContext().OpenFromActor(canvasActor);
            }
        }

        // Handle the click event for the scene view panel to select an actor in the scene

        Vector2 pickUV; // UV coordinates of the click within the scene view panel

        // Check if the click event occurred and get the UV coordinates
        // of the click within the scene view panel if requested
		if (!m_sceneViewPanel.ConsumePickRequest(pickUV))
		{
			return;
		}

		// Validate necessary pointers before proceeding with picking
		if (!activeScene)
		{
			return;
		}

		// Build the camera info for picking based on the current view mode (Scene or Canvas)
        const CameraInfo pickCameraInfo = BuildViewportCameraInfo(sceneColor->GetWidth(), sceneColor->GetHeight());

        // Determine the render space for picking based on the current view mode
        // World(3D) for Scene View, Screen(2D) for Canvas View
        const RenderSpace targetRenderSpace =
			viewport->GetViewMode() == EditorViewportMode::Scene
            ? RenderSpace::World : RenderSpace::Screen;

        // Resolve the canvas for picking if in Canvas View mode
        Canvas* editingCanvas = nullptr;

		if (viewport->GetViewMode() == EditorViewportMode::Canvas)
        {
			editingCanvas = viewport->GetCanvasEditContext().ResolveCanvas(*activeScene);
        }

		// Get the picked Actor information
		const std::optional<ScenePickHit> hit =
			ScenePicker::Pick(*activeScene, pickCameraInfo, pickUV, targetRenderSpace, editingCanvas);

		if (hit)
		{
			StopAllEditTransactions();

			if (selection)
			{
				selection->SelectActor(hit->actorGuid);
			}
		}
		else
		{
			StopAllEditTransactions();

			if (selection)
			{
				selection->Clear();
			}
		}
	}
}

void EditorApp::RenderMenuBar()
{
	IEditorDocument* document = GetActiveEditDocument();
	SceneBase* editScene = document ? document->GetWorkingScene() : nullptr;
    MenuBar::Callbacks callbacks;

	callbacks.onSaveDocument = [this]()
	{
		if (m_editorMode != EditorMode::Edit)
		{
			return;
		}

		IEditorDocument* activeDocument = GetActiveEditDocument();

		if (activeDocument && !SaveEditorDocument(m_documentManager.GetActiveDocumentId()))
		{
			DBG("EditorApp: Save failed.");
		}
	};

	callbacks.onUndo = [this]()
	{
		if (m_editorMode != EditorMode::Edit)
		{
			return;
		}

		// Cancel the current editing transaction
		CancelTransformEdit();
		CancelRectTransformEdit();

		IEditorDocument* activeDocument = GetActiveEditDocument();

		if (activeDocument && activeDocument->Undo())
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
		if (m_editorMode != EditorMode::Edit)
		{
			return;
		}

		// Cancel the current editing transaction
		CancelTransformEdit();
		CancelRectTransformEdit();

		IEditorDocument* activeDocument = GetActiveEditDocument();

		if (activeDocument && activeDocument->Redo())
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
		if (m_editorMode != EditorMode::Edit)
		{
			return;
		}

		ProjectBuilder::ReconfigureAndBuild("101Game", "Debug");
	};

	callbacks.onReloadGameCode = [this](bool reconfigure)
	{
		if (m_editorMode != EditorMode::Edit)
		{
			return;
		}

		ReloadGameCode(reconfigure);
	};

	callbacks.onCreateScript = [this](const std::string& name, bool isBehavior)
	{
		if (m_editorMode != EditorMode::Edit)
		{
			return;
		}

		bool generated;

		if (isBehavior)
		{
			generated = BehaviorTemplateGenerator::Generate(name);
		}
		else
		{
			generated = ClassTemplateGenerator::Generate(name);
		}

		if (generated)
		{
			DBG("EditorApp: Generated %s template '%s'",
				isBehavior ? "Behavior" : "class", name.c_str());

			ReloadGameCode(true);
		}
	};
	callbacks.onCreateActorImprint = [this]()
	{
		if (m_editorMode != EditorMode::Edit || !m_pActorImprintSystem)
		{
			return;
		}

		m_actorImprintsPanel.RequestCreateDialog();
	};

	callbacks.canUndo = m_editorMode == EditorMode::Edit && document &&
		document->GetCommandHistory().CanUndo();
	callbacks.canRedo = m_editorMode == EditorMode::Edit && document &&
		document->GetCommandHistory().CanRedo();
	callbacks.canSave = m_editorMode == EditorMode::Edit && document != nullptr;
	callbacks.canModifyScripts = m_editorMode == EditorMode::Edit && editScene != nullptr;
	callbacks.canModifyActorImprints = m_editorMode == EditorMode::Edit &&
		m_pActorImprintSystem != nullptr;
	callbacks.canBuild = m_editorMode == EditorMode::Edit && editScene != nullptr;

    m_menuBar.Render(callbacks);
}

void EditorApp::RenderToolbar()
{
	IEditorDocument* document = GetActiveEditDocument();
    Toolbar::Callbacks callbacks;

    callbacks.onPlay = [this]()
        {
            m_pendingModeTransition = EditorModeTransition::EnterPlay;
        };

	callbacks.onStop = [this]()
        {
            m_pendingModeTransition = EditorModeTransition::ExitPlay;
        };
	callbacks.onShowCollidersChanged = [this](bool visible)
	{
		m_showColliders = visible;

		if (!visible)
		{
			m_colliderDebugRenderData.Clear();
		}
	};

	callbacks.canPlay = m_editorMode == EditorMode::Edit && document &&
		document->CanEnterPlay();
    callbacks.canStop = m_editorMode == EditorMode::Play && m_pPlayScene != nullptr;
	callbacks.showColliders = m_showColliders;

    m_toolbar.Render(callbacks);
}

void EditorApp::RenderScriptsPanel()
{
	SceneBase* editScene = GetEditScene();
    ScriptsPanel::Callbacks callbacks;

	callbacks.onDelete = [this](const std::string& name)
	{
		if (m_editorMode != EditorMode::Edit)
		{
			return;
		}

		DeleteScript(name);
	};

	callbacks.onOpen = [](const std::string& name)
        {
			namespace fs = std::filesystem;

            const std::string basePath = PathManager::Resolve("Game/GameCode/" + name);

			const std::string headerPath = basePath + ".h";
			const std::string sourcePath = basePath + ".cpp";

			const auto openFile = [&name](const std::string& path)
				{
					if (!fs::exists(path))
					{
						DBG("EditorApp: File '%s' does not exist.", path.c_str());
						return;
					}

					ShellExecuteA(
						nullptr,
						"open",
						path.c_str(),
						nullptr,
						nullptr,
						SW_SHOWNORMAL
					);

					if (GetLastError() != ERROR_SUCCESS)
					{
						DBG("EditorApp: Failed to open file '%s' in default editor.", path.c_str());
					}
				};

            openFile(headerPath);
            openFile(sourcePath);

            DBG("EditorApp: Opening %s in default editor", name.c_str());
        };

	callbacks.canDelete = m_editorMode == EditorMode::Edit && editScene != nullptr;

	m_scriptsPanel.Render(callbacks, editScene);
}

void EditorApp::RenderActorImprintsPanel()
{
	if (!m_pAssetManager)
	{
		return;
	}

	ActorImprintsPanel::Callbacks callbacks;
	callbacks.canModify = m_editorMode == EditorMode::Edit && m_pActorImprintSystem != nullptr;
	callbacks.onCreate = [this](std::string_view name)
	{
		if (m_editorMode != EditorMode::Edit || !m_pAssetManager)
		{
			return false;
		}

		Guid assetGuid;

		if (!ActorImprintAssetWorkflow::Create(
			name, *m_pAssetManager, m_engineContext, assetGuid))
		{
			return false;
		}

		m_actorImprintsPanel.Select(assetGuid);

		ProcessActorImprintAssetChanges();
		return true;
	};
	callbacks.onEdit = [this](const Guid& assetGuid)
	{
		if (m_editorMode != EditorMode::Edit || !m_pAssetManager ||
			!m_pActorImprintSystem)
		{
			return false;
		}

		StopAllEditTransactions();
		EditorDocumentId documentId;

		if (!ActorImprintAssetWorkflow::OpenDocument(
			assetGuid, *m_pAssetManager, *m_pActorImprintSystem, m_engineContext,
			m_documentManager, m_window.GetWidth(), m_window.GetHeight(), documentId))
		{
			return false;
		}

		m_selectionRenderData.Clear();

		return true;
	};
	callbacks.onDelete = [this](const Guid& assetGuid)
	{
		if (m_editorMode != EditorMode::Edit || !m_pAssetManager ||
			!m_pActorImprintSystem)
		{
			return false;
		}

		if (!ActorImprintAssetWorkflow::Delete(
			assetGuid, *m_pAssetManager, *m_pActorImprintSystem,
			m_documentManager))
		{
			return false;
		}

		ProcessActorImprintAssetChanges();
		return true;
	};
	m_actorImprintsPanel.Render(*m_pAssetManager, callbacks);
}

void EditorApp::RenderSceneAssetPanel()
{
	if (!m_pAssetManager)
	{
		return;
	}

	SceneAssetPanel::Callbacks callbacks;
	callbacks.canModify = m_editorMode == EditorMode::Edit;
	callbacks.onCreate = [this](std::string_view name)
	{
		return RequestCreateScene(name);
	};
	callbacks.onOpen = [this](const Guid& guid)
	{
		if (!RequestOpenScene(guid))
		{
			m_sceneAssetPanel.SetDiagnostic("Scene could not be loaded. The current Scene was preserved.");
			return false;
		}

		m_sceneAssetPanel.SetDiagnostic("Scene opened.");
		return true;
	};
	callbacks.onRename = [this](const Guid& guid, std::string_view name)
	{
		std::string path;
		SceneAssetWorkflowError error;

		if (!SceneAssetWorkflow::Rename(guid, name, *m_pAssetManager, path, &error))
		{
			m_sceneAssetPanel.SetDiagnostic(error.message);
			return false;
		}

		for (const EditorDocumentInfo& info : m_documentManager.GetDocuments())
		{
			if (info.type != EditorDocumentType::Scene || info.sourceAssetGuid != guid)
			{
				continue;
			}

			auto* document = static_cast<SceneEditorDocument*>(m_documentManager.FindDocument(info.id));

			if (document)
			{
				document->UpdateAssetPath(guid, path);
			}
		}

		m_sceneAssetPanel.SetDiagnostic("Scene asset renamed.");
		return true;
	};
	callbacks.onSetEditorStartup = [this](const Guid& guid)
	{
		const AssetEntry* entry = m_pAssetManager->GetAssetEntry(guid);

		if (!entry || entry->type != AssetType::Scene)
		{
			m_sceneAssetPanel.SetDiagnostic("Editor Startup Scene must reference a Scene asset.");
			return false;
		}

		ProjectSettings candidate = m_projectSettings;
		candidate.SetEditorStartupSceneGuid(guid);

		std::string error;

		if (!candidate.Save(PathManager::Resolve("project.101"), &error))
		{
			m_sceneAssetPanel.SetDiagnostic(error);
			return false;
		}

		m_projectSettings = candidate;
		m_sceneAssetPanel.SetDiagnostic("Editor Startup Scene updated.");

		return true;
	};

	callbacks.onSetGameStartup = [this](const Guid& guid)
	{
		const AssetEntry* entry = m_pAssetManager->GetAssetEntry(guid);

		if (!entry || entry->type != AssetType::Scene)
		{
			m_sceneAssetPanel.SetDiagnostic("Game Startup Scene must reference a Scene asset.");
			return false;
		}

		ProjectSettings candidate = m_projectSettings;
		candidate.SetGameStartupSceneGuid(guid);

		std::string error;

		if (!candidate.Save(PathManager::Resolve("project.101"), &error))
		{
			m_sceneAssetPanel.SetDiagnostic(error);
			return false;
		}

		m_projectSettings = candidate;
		m_sceneAssetPanel.SetDiagnostic("Game Startup Scene updated.");
		return true;
	};

	m_sceneAssetPanel.Render(
		*m_pAssetManager,
		m_projectSettings.GetEditorStartupSceneGuid(),
		m_projectSettings.GetGameStartupSceneGuid(),
		callbacks);
}

void EditorApp::RenderTagManagerPanel()
{
	TagManagerPanel::Callbacks callbacks;
	auto report = [this](const TagManagementResult& result)
	{
		if (result)
		{
			m_operationDiagnostic = "Project Tags updated.";
		}
		else
		{
			m_operationDiagnostic = result.error;

			for (const TagUsage& usage : result.usages)
			{
				m_operationDiagnostic += "\n- " + usage.location;
			}
		}

		m_openOperationDiagnosticPopup = true;
		return static_cast<bool>(result);
	};
	const std::string settingsPath = PathManager::Resolve("project.101");
	callbacks.onCreate = [this, settingsPath, report](const std::string& name)
	{
		return report(TagManagementWorkflow::Create(name, m_projectSettings, settingsPath));
	};
	callbacks.onRename = [this, settingsPath, report](const std::string& oldName, const std::string& newName)
	{
		return report(TagManagementWorkflow::Rename(oldName, newName, m_projectSettings,
			settingsPath, *m_pAssetManager, m_documentManager));
	};
	callbacks.onDelete = [this, settingsPath, report](const std::string& name)
	{
		return report(TagManagementWorkflow::Delete(name, m_projectSettings,
			settingsPath, *m_pAssetManager, m_documentManager));
	};
	m_tagManagerPanel.Render(m_projectSettings.GetUserTags(), callbacks);
}

void EditorApp::RenderDocumentDecisionModal()
{
	if (m_openDocumentDecisionPopup)
	{
		ImGui::OpenPopup("Unsaved Document");
		m_openDocumentDecisionPopup = false;
	}

	if (!ImGui::BeginPopupModal("Unsaved Document", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	if (m_pendingSceneAction != PendingSceneAction::None)
	{
		IEditorDocument* sceneDocument = m_documentManager.FindFirst(EditorDocumentType::Scene);
		const std::string sceneDisplayName = sceneDocument
			? std::string(sceneDocument->GetDisplayName())
			: "Scene";
		ImGui::Text("Save changes to '%s'?", sceneDisplayName.c_str());
		ImGui::TextUnformatted("The requested Scene operation will replace the current Scene.");

		if (!m_documentDecisionDiagnostic.empty())
		{
			ImGui::TextWrapped("%s", m_documentDecisionDiagnostic.c_str());
		}

		auto clearPending = [this]()
		{
			m_pendingSceneAction = PendingSceneAction::None;
			m_pendingSceneName.clear();
			m_pendingSceneGuid = {};
		};

		if (ImGui::Button("Save", ImVec2(120, 0)))
		{
			EditorDocumentId id;

			for (const auto& info : m_documentManager.GetDocuments())
			{
				if (m_documentManager.FindDocument(info.id) == sceneDocument)
				{
					id = info.id;
					break;
				}
			}

			if (!id.IsValid() || !SaveEditorDocument(id))
			{
				m_documentDecisionDiagnostic = "Save failed. The current Scene was preserved.";
			}
			else
			{
				m_documentDecisionDiagnostic.clear();
				ExecutePendingSceneAction();
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("Discard", ImVec2(120, 0)))
		{
			m_documentDecisionDiagnostic.clear();
			ExecutePendingSceneAction();
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			clearPending();
			m_documentDecisionDiagnostic.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
		return;
	}

	IEditorDocument* document = m_documentManager.FindDocument(m_documentWorkflow.GetPendingDocumentId());

	if (!document)
	{
		m_documentWorkflow.CancelPending();
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	ImGui::Text("Save changes to '%s'?",
		std::string(document->GetDisplayName()).c_str());
	const char* closeExplanation = m_documentWorkflow.IsExitPending()
		? "The Editor will exit after every modified document is resolved."
		: "Closing this document will discard unsaved changes.";
	ImGui::TextUnformatted(closeExplanation);

	if (!m_documentDecisionDiagnostic.empty())
	{
		ImGui::TextWrapped("%s", m_documentDecisionDiagnostic.c_str());
	}

	const auto resolve = [this](EditorDocumentCloseDecision decision)
	{
		EditorDocumentWorkflowResult result = m_documentWorkflow.ResolvePending(decision);

		if (result == EditorDocumentWorkflowResult::SaveFailed)
		{
			m_documentDecisionDiagnostic = "Save failed. The document remains open.";
			return;
		}

		m_documentDecisionDiagnostic.clear();

		if (result == EditorDocumentWorkflowResult::ConfirmationRequired)
		{
			return;
		}

		if (result == EditorDocumentWorkflowResult::ExitReady)
		{
			ImGui::CloseCurrentPopup();
			CompleteExitRequest();
			return;
		}

		if (result == EditorDocumentWorkflowResult::Closed)
		{
			m_selectionRenderData.Clear();
		}

		ImGui::CloseCurrentPopup();
	};

	if (ImGui::Button("Save", ImVec2(120, 0)))
	{
		resolve(EditorDocumentCloseDecision::Save);
	}

	ImGui::SameLine();

	if (ImGui::Button("Discard", ImVec2(120, 0)))
	{
		resolve(EditorDocumentCloseDecision::Discard);
	}

	ImGui::SameLine();

	if (ImGui::Button("Cancel", ImVec2(120, 0)))
	{
		resolve(EditorDocumentCloseDecision::Cancel);
	}

	ImGui::EndPopup();
}

void EditorApp::RenderOperationDiagnosticModal()
{
	if (m_openOperationDiagnosticPopup)
	{
		ImGui::OpenPopup("Editor Operation Failed");
		m_openOperationDiagnosticPopup = false;
	}

	if (!ImGui::BeginPopupModal("Editor Operation Failed", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	ImGui::TextWrapped("%s", m_operationDiagnostic.c_str());

	if (ImGui::Button("OK", ImVec2(120, 0)))
	{
		m_operationDiagnostic.clear();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

bool EditorApp::HandleWindowCloseRequest()
{
	if (m_allowWindowClose)
	{
		return false;
	}

	StopAllEditTransactions();
	const EditorDocumentWorkflowResult result = m_documentWorkflow.RequestExit();

	if (result == EditorDocumentWorkflowResult::ExitReady)
	{
		m_allowWindowClose = true;
		return false;
	}

	if (result == EditorDocumentWorkflowResult::ConfirmationRequired)
	{
		m_documentDecisionDiagnostic.clear();
		m_openDocumentDecisionPopup = true;
	}

	return true;
}

void EditorApp::CompleteExitRequest()
{
	m_allowWindowClose = true;
	PostMessageW(m_window.GetHandle(), WM_CLOSE, 0, 0);
}

void EditorApp::ShutdownImGui()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

SceneBase* EditorApp::GetActiveScene() const
{
	if (m_editorMode == EditorMode::Edit)
	{
		return GetEditScene();
	}

	if (m_editorMode == EditorMode::Play && m_pPlayScene)
	{
		return m_pPlayScene;
	}

	return nullptr;
}

IEditorDocument* EditorApp::GetActiveEditDocument()
{
	return m_documentManager.GetActiveDocument();
}

const IEditorDocument* EditorApp::GetActiveEditDocument() const
{
	return m_documentManager.GetActiveDocument();
}

SceneBase* EditorApp::GetEditScene() const
{
	const IEditorDocument* document = GetActiveEditDocument();
	return document ? const_cast<SceneBase*>(document->GetWorkingScene()) : nullptr;
}

EditorSelection* EditorApp::GetActiveSelection()
{
	IEditorDocument* document = GetActiveEditDocument();
	return document ? &document->GetSelection() : nullptr;
}

EditorViewportContext* EditorApp::GetActiveViewportContext()
{
	IEditorDocument* document = GetActiveEditDocument();
	return document ? &document->GetViewportContext() : nullptr;
}

const EditorViewportContext* EditorApp::GetActiveViewportContext() const
{
	const IEditorDocument* document = GetActiveEditDocument();
	return document ? &document->GetViewportContext() : nullptr;
}

void EditorApp::ApplySceneViewResizeRequest()
{
	EditorViewportContext* viewport = GetActiveViewportContext();
	SceneBase* editScene = GetEditScene();

	if (!viewport)
	{
		return;
	}

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
	Camera& editorCamera = viewport->GetSceneCamera().GetCamera();
	CameraLens lens = editorCamera.GetCameraLens();
	lens.width = static_cast<float>(width);
	lens.height = static_cast<float>(height);

	editorCamera.SetCameraLens(lens);

    // Apply the viewport size to the scene and invalidate
    // all layout elements affected by the size change
	if (editScene)
    {
		editScene->SetViewportSize(width, height);
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
	if (m_editorMode != EditorMode::Edit)
	{
		return;
	}

	SceneBase* editScene = GetEditScene();

	// Validate the actorGuid
	if (!editScene || !actorGuid.IsValid())
    {
		CancelTransformEdit();
        return;
    }

	Actor* actor = editScene->ResolveActor(actorGuid);

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
	if (m_editorMode != EditorMode::Edit)
	{
		return;
	}

	// Validate that there is an ongoing transform edit transaction and that the actorGuid matches
	if (!m_transformEditTransaction)
	{
		return;
	}

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

	IEditorDocument* document = GetActiveEditDocument();
	SceneBase* editScene = document ? document->GetWorkingScene() : nullptr;

	if (!document || !editScene)
	{
		return;
	}

	// Add a TransformEditCommand to the active Document history.
	document->ExecuteCommand(
        std::make_unique<TransformEditCommand>(
			editScene,
            actorGuid,
            before,
            after
        )
    );
}

void EditorApp::CancelTransformEdit()
{
	if (!m_transformEditTransaction)
	{
		return;
	}

	const Guid actorGuid = m_transformEditTransaction->actorGuid;
	SceneBase* editScene = GetEditScene();

	if (!editScene || !actorGuid.IsValid())
    {
        m_transformEditTransaction.reset();
        return;
    }

	Actor* actor = editScene->ResolveActor(actorGuid);

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
	if (m_editorMode != EditorMode::Edit)
	{
		return;
	}

	SceneBase* editScene = GetEditScene();

	if (!editScene || !actorGuid.IsValid())
	{
		CancelRectTransformEdit();
		return;
	}

	Actor* actor = editScene->ResolveActor(actorGuid);

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
	if (m_editorMode != EditorMode::Edit)
	{
		return;
	}

	if (!m_rectTransformEditTransaction)
	{
		return;
	}

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
	IEditorDocument* document = GetActiveEditDocument();
	SceneBase* editScene = document ? document->GetWorkingScene() : nullptr;

	if (!document || !editScene)
	{
		return;
	}

	document->ExecuteCommand(
        std::make_unique<RectTransformEditCommand>(
		    editScene,
		    actorGuid,
		    before,
		    after
	    )
	);
}

void EditorApp::CancelRectTransformEdit()
{
	if (!m_rectTransformEditTransaction)
	{
		return;
	}

	const Guid actorGuid = m_rectTransformEditTransaction->actorGuid;
	SceneBase* editScene = GetEditScene();

	if (!editScene || !actorGuid.IsValid())
    {
        m_rectTransformEditTransaction.reset();
        return;
    }

	Actor* actor = editScene->ResolveActor(actorGuid);

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

	if (!activeScene)
	{
		return;
	}

	EditorSelection* selection = GetActiveSelection();
	Actor* selectedActor = selection ? selection->ResolveActor(activeScene) : nullptr;

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
			if (isCanvasView)
			{
				item.common.worldMatrix *= worldToCanvas;
			}

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
		if (isCanvasView)
		{
			item.common.worldMatrix *= worldToCanvas;
		}

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
			if (isCanvasView)
			{
				item.common.worldMatrix *= worldToCanvas;
			}

			m_selectionRenderData.AddUI(std::move(item));
        }
	}
}

void EditorApp::BuildColliderDebugRenderData(SceneBase* scene)
{
	m_colliderDebugRenderData.Clear();

	if (!m_showColliders || !scene || !m_pMeshManager)
	{
		return;
	}

	for (Actor* actor : scene->GetAllActors())
	{
		if (!actor || !actor->IsActive() || actor->IsDestroyed())
		{
			continue;
		}

		for (Collider* collider : actor->GetComponentsByClass<Collider>())
		{
			if (!collider || !collider->isActive() ||
				collider->GetType() == ColliderType::None)
			{
				continue;
			}

			collider->Flush();

			if (collider->GetType() == ColliderType::CAPSULE)
			{
				const CapsuleCollider& capsule = collider->GetCurrentCapsuleCollider();
				const float diameter = capsule.radius * 2.0f;
				const Quaternion rotation = collider->GetWorldTransformCurrent().rotation;
				const MeshHandle sphereMesh = m_pMeshManager->LoadDefaultMesh(DefaultMesh::Sphere);

				auto addMesh = [this](MeshHandle mesh, const Matrix4x4& world)
					{
						MeshRenderItem item{};
						item.meshDesc.meshHandle = mesh;
						item.common.worldMatrix = world;
						m_colliderDebugRenderData.AddMeshs(std::move(item));
					};

				if (capsule.cylHeight <= 0.0f)
				{
					addMesh(sphereMesh, Matrix4x4::CreateTRS(
						collider->GetWorldTransformCurrent().position,
						Quaternion::Identity(),
						Vector3(diameter, diameter, diameter)));
				}
				else
				{
					const MeshHandle cylinderMesh =
						m_pMeshManager->LoadDefaultMesh(DefaultMesh::Cylinder);
					addMesh(cylinderMesh, Matrix4x4::CreateTRS(
						collider->GetWorldTransformCurrent().position,
						rotation,
						Vector3(diameter, capsule.cylHeight, diameter)));
					addMesh(sphereMesh, Matrix4x4::CreateTRS(
						capsule.pointA, Quaternion::Identity(),
						Vector3(diameter, diameter, diameter)));
					addMesh(sphereMesh, Matrix4x4::CreateTRS(capsule.pointB, Quaternion::Identity(),
											Vector3(diameter, diameter, diameter)));
				}

				continue;
			}

			DefaultMesh meshType;
			switch (collider->GetType())
			{
			case ColliderType::BOX:
				meshType = DefaultMesh::Cube;
				break;
			case ColliderType::SPHERE:
				meshType = DefaultMesh::Sphere;
				break;
			default: continue;
			}

			MeshRenderItem item{};
			item.meshDesc.meshHandle = m_pMeshManager->LoadDefaultMesh(meshType);
			item.common.worldMatrix = collider->GetWorldMatrix();
			m_colliderDebugRenderData.AddMeshs(std::move(item));
		}
	}
}

CameraInfo EditorApp::BuildViewportCameraInfo(UINT viewportWidth, UINT viewportHeight)
{
	// Avoid zero division and invalid viewport sizes
	if (viewportWidth == 0 || viewportHeight == 0)
	{
		return {};
	}

	EditorViewportContext* viewport = GetActiveViewportContext();

	if (!viewport)
	{
		return {};
	}

	// Decide projection type based on the current viewport mode (Scene or Canvas)

	if (viewport->GetViewMode() == EditorViewportMode::Scene)
	{
		// Use the editor camera's perspective projection for Scene View
		return viewport->GetSceneCamera().GetCamera().GetCameraInfo();
	}

	// From here on, build an orthographic projection for Canvas View
    // based on the canvas reference size and viewport size

    const Vector2 fitExtent = CalculateCanvasViewExtent(viewportWidth, viewportHeight);
	const CanvasViewNavigation& navigation = viewport->GetCanvasNavigation();
	const Vector2 visibleExtent = fitExtent / navigation.zoom;

    CameraInfo cameraInfo{};
    cameraInfo.position = { 0.0f, 0.0f, -1.0f };
    cameraInfo.forward = Vector3::Forward();
    cameraInfo.up = Vector3::Up();

    cameraInfo.viewMatrix =
        Matrix4x4::CreateTranslation(
            {
				-navigation.center.x,
				-navigation.center.y,
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
            500.0f
        );

    return cameraInfo;
}

ViewportOverlayData EditorApp::BuildViewportOverlayData(UINT viewportWidth, UINT viewportHeight)
{
	ViewportOverlayData overlayData;
	EditorViewportContext* viewport = GetActiveViewportContext();
	EditorSelection* selection = GetActiveSelection();

	SceneBase* activeScene = GetActiveScene();

	if (!viewport || !activeScene || viewportWidth == 0 || viewportHeight == 0)
	{
		return overlayData;
	}

	const CameraInfo cameraInfo = BuildViewportCameraInfo(viewportWidth, viewportHeight);
	const Matrix4x4 viewProjection = cameraInfo.viewMatrix * cameraInfo.projMatrix;

	// Built overlay data for the Canvas View mode
	// In Canvas View, we need to display the editing canvas and ancestor Canvases of the selected Canvas up to the editing canvas
	if (viewport->GetViewMode() == EditorViewportMode::Canvas)
	{
		// Resolve editing canvas from the CanvasEditContext, which is the root canvas for the Canvas View
		Canvas* editingCanvas = viewport->GetCanvasEditContext().ResolveCanvas(*activeScene);

		if (!editingCanvas)
		{
			return overlayData;
		}

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

		const auto appendBreadcrumb = [&overlayData](Canvas* canvas)
		{
			Actor* canvasActor = canvas ? canvas->GetOwner() : nullptr;

			if (!canvasActor)
			{
				return;
			}

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
		const auto appendRect = [&overlayData, &viewProjection](
									const Vector3(&corners)[4], const Matrix4x4& transform, ViewportCanvasRole role)
		{
			ViewportCanvasRect rect;
			rect.role = role;

			for (size_t i = 0; i < 4; i++)
			{
				const Vector3 position = transform.TransformPoint(corners[i]);
				const Vector3 ndc = viewProjection.TransformPoint(position);
				rect.corners[i] = {ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f};
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
		Actor* selectedActor = selection ? selection->ResolveActor(activeScene) : nullptr;
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

					if (canvas == editingCanvas)
					{
						break;
					}
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

				if (!rect)
				{
					continue;
				}

				// World space to editing canvas space transform
				Matrix4x4 transform = rect->GetWorldMatrix() * worldToEditingCanvas;

				// Determine role if the current Canvas is selected or an ancestor for overlay highlighting
				ViewportCanvasRole role =
					(i + 1 == path.size()) ? ViewportCanvasRole::Selected : ViewportCanvasRole::Ancestor;

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
	Actor* selectedActor = selection ? selection->ResolveActor(activeScene) : nullptr;
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
		if (!actor || !actor->IsActive() || actor->IsDestroyed())
		{
			continue;
		}

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
			frameMatrix =
				Matrix4x4::CreateScale({referenceSize.x, referenceSize.y, 1.0f}) * canvas->GetContentWorldMatrix();
		}
		else
		{// Child Canvas
			RectTransform* rectTransform = actor->GetComponentByClass<RectTransform>();

			if (!rectTransform)
			{
				continue;
			}

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
	EditorViewportContext* viewport = GetActiveViewportContext();

	if (!viewport)
	{
		return;
	}

	CanvasViewNavigation& navigation = viewport->GetCanvasNavigation();
	Actor* canvasActor = editingCanvas ? editingCanvas->GetOwner() : nullptr;

	const Guid canvasGuid = canvasActor ? canvasActor->GetGuid() : Guid{};

	// Check if the stored Canvas Guid for manipulation is the same as the current editing canvas.
	if (navigation.canvasActorGuid == canvasGuid)
	{
		return;
	}

	// Reset the canvas view navigation state when switching to a different canvas in the Canvas View mode
	navigation.canvasActorGuid = canvasGuid;
	navigation.center = Vector2::Zero();
	navigation.zoom = 1.0f;
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
	const EditorViewportContext* viewport = GetActiveViewportContext();

	if (viewport && activeScene)
	{
		Canvas* canvas = viewport->GetCanvasEditContext().ResolveCanvas(*activeScene);

		if (canvas)
		{
			referenceSize = canvas->GetLayoutReferenceSize();
		}
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

void EditorApp::ApplySceneNavigationInput(
    const SceneNavigationInput& input,
    float deltaTime)
{
	EditorViewportContext* viewport = GetActiveViewportContext();

	if (!viewport)
	{
		return;
	}

	EditorViewCamera& camera = viewport->GetSceneCamera();

	if (input.lookDeltaPixels.LengthSq() > 0.0f)
    {
		camera.Look(
            input.lookDeltaPixels,
            kLookRadiansPerPixel,
            kMaximumPitchRadians);
    }

    if (input.flyDirection.LengthSq() > 0.0f)
    {
        const float speedMultiplier = input.fastMove ? kFastMoveMultiplier : 1.0f;
        const float distance =
			camera.GetNavigationState().moveSpeed *
            speedMultiplier * deltaTime;

		camera.Fly(input.flyDirection, distance);
    }

    if (input.panDeltaPixels.LengthSq() > 0.0f)
    {
		camera.Pan(
            input.panDeltaPixels,
            kPanDistanceScalePerPixel);
    }

    if (input.orbitDeltaPixels.LengthSq() > 0.0f)
    {
		camera.Orbit(
            input.orbitDeltaPixels,
            kOrbitRadiansPerPixel,
            kMaximumPitchRadians);
    }

    if (input.wheelDelta != 0.0f)
    {
		camera.Dolly(
            input.wheelDelta,
            kDollyDistanceFractionPerStep,
            kMinimumPivotDistance);
    }

    if (input.focusRequested)
	{
		SceneBase* scene = GetActiveScene();

		if (scene)
        {
            const std::optional<EditorCameraFocusBounds> bounds =
                BuildSelectedActorFocusBounds(*scene);

            if (bounds)
            {
				camera.Focus(
                    *bounds,
                    kFocusPadding,
                    kMinimumPivotDistance);
            }
        }
	}
}

std::optional<EditorCameraFocusBounds> EditorApp::BuildSelectedActorFocusBounds(
    SceneBase& scene)
{
	EditorSelection* selection = GetActiveSelection();
	Actor* selectedActor = selection ? selection->ResolveActor(&scene) : nullptr;

	if (!selectedActor || selectedActor->IsDestroyed())
	{
		return std::nullopt;
	}

	Vector3 boundsMin;
    Vector3 boundsMax;
    bool hasBounds = false;

    for (MeshRenderer* renderer : selectedActor->GetComponentsByClass<MeshRenderer>())
    {
		if (!renderer || !renderer->IsVisible() || !renderer->IsConfigured())
		{
			continue;
		}

		const MeshRendererProxy& proxy = renderer->GetRenderProxy();

		if (proxy.common.renderSpace != RenderSpace::World)
		{
			continue;
		}

		Vector3 worldPosition;
		Quaternion worldRotation;
		Vector3 worldScale;

		if (!proxy.common.worldMatrix.Decompose(worldPosition, worldRotation, worldScale))
		{
			continue;
		}

		const float maximumScale = std::max({
            std::abs(worldScale.x),
            std::abs(worldScale.y),
            std::abs(worldScale.z)
        });

        for (const SubmeshRenderTemplate& renderTemplate : renderer->GetRenderTemplates())
        {
            const MeshDesc& mesh = renderTemplate.meshDesc;
            const Vector3 center = proxy.common.worldMatrix.TransformPoint(mesh.boundsCenter);
            const float radius = std::max(0.0f, mesh.boundsRadius * maximumScale);
            const Vector3 extent(radius);
            const Vector3 sphereMin = center - extent;
            const Vector3 sphereMax = center + extent;

            if (!hasBounds)
            {
                boundsMin = sphereMin;
                boundsMax = sphereMax;
                hasBounds = true;
            }
            else
            {
                boundsMin = boundsMin.Min(sphereMin);
                boundsMax = boundsMax.Max(sphereMax);
            }
        }
    }

	if (!hasBounds)
	{
		return std::nullopt;
	}

	EditorCameraFocusBounds result;
    result.center = (boundsMin + boundsMax) * 0.5f;
    result.radius = (boundsMax - result.center).Length();
    return result;
}

void EditorApp::ApplyCanvasNavigationInput(const CanvasNavigationInput& input, UINT viewportWidth, UINT viewportHeight)
{
	EditorViewportContext* viewport = GetActiveViewportContext();

	if (!viewport)
	{
		return;
	}

	CanvasViewNavigation& navigation = viewport->GetCanvasNavigation();
    // Fitting the viewport to Editing-Root Canvas (No pan and zoom)
    if (input.fitRequested)
    {
		navigation.center = Vector2::Zero();
		navigation.zoom = 1.0f;
        return;
    }

	// Avoid zero division and invalid viewport sizes
	if (viewportWidth == 0 || viewportHeight == 0)
	{
		return;
	}

	// Get the extent without zoom
    const Vector2 fitExtent = CalculateCanvasViewExtent(viewportWidth, viewportHeight);

	// Calculate the extent considering the current zoom level
	const Vector2 currentExtent = fitExtent / navigation.zoom;

    // Calculate center position considering the amount of the pan manipulation
	navigation.center.x -= input.panDeltaPixels.x * currentExtent.x / static_cast<float>(viewportWidth);
	navigation.center.y += input.panDeltaPixels.y * currentExtent.y / static_cast<float>(viewportHeight);

	if (input.wheelDelta == 0.0f)
	{
		return; // End if there is no zoom input
	}

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
	const float newZoom = std::clamp(navigation.zoom * std::pow(1.1f, input.wheelDelta), kMinZoom, kMaxZoom);

    // The extent after zooming
    const Vector2 newExtent = fitExtent / newZoom;

	// Calculate the pivot offset after zooming
    const Vector2 pivotOffsetAfter
    {
        (input.zoomPivotUV.x - 0.5f) * newExtent.x,
        (0.5f - input.zoomPivotUV.y) * newExtent.y
    };

	// Adjust the center position to keep the zoom pivot point fixed in the viewport
	navigation.center += pivotOffsetBefore - pivotOffsetAfter;

	navigation.zoom = newZoom;
}

void EditorApp::StopAllEditTransactions()
{
	m_inspectorPanel.CancelActiveEdit();
	CancelTransformEdit();
	CancelRectTransformEdit();
}
