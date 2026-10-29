#pragma once
#include <windows.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/RenderAssetResources.h"
#include "Rendering/EditorRenderAssets.h"
#include "Rendering/EditorImGuiAdapter.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Window/Window.h"
#include "Command/RectTransformEditCommand.h"
#include "Document/EditorDocumentManager.h"
#include "Document/EditorDocumentWorkflow.h"
#include "UI/HierarchyPanel.h"
#include "UI/Inspector/InspectorPanel.h"
#include "UI/SceneViewPanel.h"
#include "UI/MenuBar.h"
#include "UI/Toolbar.h"
#include "UI/ScriptsPanel.h"
#include "UI/ActorImprintsPanel.h"
#include "UI/SceneAssetPanel.h"
#include "UI/TagManagerPanel.h"
#include "Engine/Project/ProjectSettings.h"

//--------------------------------------------
// EditorApp class
// The main application class for the editor.
//--------------------------------------------

// Enumeration to represent the current mode of the editor (Edit or Play)
enum class EditorMode
{
	Edit,   // Edit mode for editing the scene
	Play    // Play mode for testing the game
};

class EditorApp
{
    // Fixed resolution for the play viewport
    // This should be moved to the data driven configuration in the future (e.g., from a project settings file)
    static constexpr UINT PLAY_VIEWPORT_WIDTH = 1920;
    static constexpr UINT PLAY_VIEWPORT_HEIGHT = 1080;

public:
    EditorApp(const EditorApp&) = delete;
    EditorApp& operator=(const EditorApp&) = delete;
    EditorApp(EditorApp&&) = delete;
    EditorApp& operator=(EditorApp&&) = delete;

    static EditorApp* GetInstance()
    {
        static EditorApp instance;
        return &instance;
    }

    bool Initialize();
    // Return the actual loop outcome so a rendering failure is not reported
    // as a successful process exit. Terminate remains the single cleanup path.
    int Run();
    void Terminate();

private:
    enum class EditorModeTransition
    {
        None,
        EnterPlay,
        ExitPlay
    };

    Window m_window;

    // Sticky for one application run. Posting WM_QUIT alone does not unwind
    // Update/Render, so every frame boundary also observes this stop condition.
    // It does not own resources and does not change Edit synchronization timing.
    bool m_renderingStopped = false;

    std::unique_ptr<rendering::Renderer> m_pRenderer;
    rendering::WindowOutputHandle m_windowOutput;
    std::unique_ptr<RenderAssetResources> m_renderAssets;
    std::unique_ptr<EditorRenderAssets> m_editorRenderAssets;
    std::unique_ptr<rendering::editor::EditorImGuiAdapter> m_imguiAdapter;
    ImTextureID m_sceneImage = ImTextureID_Invalid;
    std::unique_ptr<AssetManager> m_pAssetManager;
    std::unique_ptr<ActorImprintSystem> m_pActorImprintSystem;

    InputManager& m_inputManager = InputManager::GetInstance();
    TimeManager& m_timeManager = TimeManager::GetInstance();

    EngineContext m_engineContext;

    EditorMode m_editorMode = EditorMode::Edit; // The current mode of the editor (Edit or Play)
    EditorModeTransition m_pendingModeTransition = EditorModeTransition::None;
	float m_assetRefreshElapsedSeconds = 0.0f;
    std::unique_ptr<SceneManager> m_pPlaySceneManager;
    SceneBase* m_pPlayScene = nullptr; // Non-owning current Scene of m_pPlaySceneManager.
	EditorDocumentManager m_documentManager;
	EditorDocumentWorkflow m_documentWorkflow{m_documentManager};
	bool m_openDocumentDecisionPopup = false;
	bool m_allowWindowClose = false;
    std::string m_documentDecisionDiagnostic;
	enum class PendingSceneAction { None, Create, Open };
	PendingSceneAction m_pendingSceneAction = PendingSceneAction::None;
	std::string m_pendingSceneName;
	Guid m_pendingSceneGuid;

    // Panels
    HierarchyPanel m_hierarchyPanel;
    InspectorPanel m_inspectorPanel;
    SceneViewPanel m_sceneViewPanel;
    MenuBar m_menuBar;
    Toolbar m_toolbar;
    ScriptsPanel m_scriptsPanel;
    ActorImprintsPanel m_actorImprintsPanel;
	SceneAssetPanel m_sceneAssetPanel;
	TagManagerPanel m_tagManagerPanel;
	ProjectSettings m_projectSettings;

    // ImGui keeps IniFilename as a raw pointer, so the backing string must
    // remain alive for the entire ImGui context lifetime.
    std::string m_imguiIniPath;
    bool m_shouldBuildDefaultDockLayout = false;

    HMODULE m_hGameCodeDll = nullptr;    // Handle to the loaded game code DLL (for hot-reloading)

    // Render data for rendering the outline of a selected object in the scene view
	bool m_showColliders = false;

private:
    // Struct to track transform edits for undo/redo
    struct TransformEditTransaction
    {
        Guid actorGuid;
        Transform3D before;
    };

    // Optional to track an ongoing transform edit transaction
    std::optional<TransformEditTransaction> m_transformEditTransaction;

    // Helper functions for callbacks from the InspectorPanel to track transform edits for undo/redo
    void BeginTransformEdit(const Guid& actorGuid, const Transform3D& before);
    void EndTransformEdit(const Guid& actorGuid, const Transform3D& after);
    void CancelTransformEdit();

    // Struct to track RectTransform edits for undo/redo
    struct RectTransformEditTransaction
    {
        Guid actorGuid;
        RectTransformEditState before;
    };

    // Optional to track an ongoing RectTransform edit transaction
    std::optional<RectTransformEditTransaction> m_rectTransformEditTransaction;

    // Helper functions for callbacks from the InspectorPanel to track RectTransform edits for undo/redo
    void BeginRectTransformEdit(const Guid& actorGuid, const RectTransformEditState& before);
    void EndRectTransformEdit(const Guid& actorGuid, const RectTransformEditState& after);
    void CancelRectTransformEdit();

private:
    EditorApp() = default;

	bool ApplyWindowResizeRequest();

    void NewScene();                                // Create a new scene with default settings
    void LoadScene(const std::string& filePath);    // Load a scene from a file
	bool LoadScene(const Guid& sceneAssetGuid);
	std::unique_ptr<SceneBase> CreateDefaultScene();
	bool RequestCreateScene(std::string_view name);
	bool RequestOpenScene(const Guid& sceneAssetGuid);
	bool ExecutePendingSceneAction();
    void ReloadGameCode(bool reconfigure);          // For hot-reloading game code DLL
    void DeleteScript(const std::string& name);     // Delete a script file from the project
    void EnterPlayMode();                           // Switch to Play mode
    void ExitPlayMode();                            // Switch back to Edit mode
    void ApplyPendingModeTransition();              // Apply a deferred Play/Edit mode transition
	void RefreshAssetCatalog(float deltaTime);
	bool ProcessActorImprintAssetChanges(const Guid* requiredAsset = nullptr);
	bool SaveEditorDocument(EditorDocumentId id);
	bool HasActiveEditTransaction() const;

    bool SaveHotReloadSnapshot();
    bool BuildStagedGameCode(bool reconfigure);
    bool DestroyCurrentRuntimeState();
    bool PromoteStagedGameCode();
    bool RestorePreviousGameCode();
    bool RollbackGameCode();
    bool RestoreHotReloadSnapshot();
    void RemovePreviousGameCodeBackup();

    void PrepareInstance();
    bool InitInstance();
    void InitImGui();

    void Update(float deltaTime);
    void Render();

    void AppendEditorViewport(rendering::FrameRenderData& frame, SceneBase* activeScene);
    bool CheckRenderingResult(const rendering::Result& result);

    void RenderImGui();
    void RenderMainDockSpace();
    void BuildDefaultDockLayout(unsigned int dockSpaceId);
    void RenderHierarchyPanel();
    void RenderInspectorPanel();
    void RenderSceneViewPanel();
    void RenderMenuBar();
	void RenderToolbar();
    void RenderScriptsPanel();
    void RenderActorImprintsPanel();
	void RenderSceneAssetPanel();
	void RenderTagManagerPanel();
	void RenderDocumentDecisionModal();
	void RenderOperationDiagnosticModal();
    void ShutdownImGui();

    SceneBase* GetActiveScene() const;
	IEditorDocument* GetActiveEditDocument();
	const IEditorDocument* GetActiveEditDocument() const;
	SceneBase* GetEditScene() const;
	EditorSelection* GetActiveSelection();
	EditorViewportContext* GetActiveViewportContext();
	const EditorViewportContext* GetActiveViewportContext() const;

    void ApplySceneViewResizeRequest();
    void ApplySceneRenderTargetSizeToScene(SceneBase& scene);

    // Helper function to build CameraInfo for the current viewport size
    // Camera matrix is built based on the current viewport mode (Scene or Canvas)
    CameraInfo BuildViewportCameraInfo(UINT viewportWidth, UINT viewportHeight);

    // Build Canvas guide rectangles for the current Scene/Canvas View mode.
    ViewportOverlayData BuildViewportOverlayData(UINT viewportWidth, UINT viewportHeight);

    // Helper function to synchronize the Canvas view navigation state with the currently editing Canvas
    // Used when the editing root Canvas is changed in the Canvas View to update the displaying state of the Canvas view
    void SyncCanvasViewNavigation(const Canvas* editingCanvas);

    // Helper funtion to calculate the extent which can fit the editing root Canvas rectangle into the viewport of Canvas View
    // The extent is in the local space of the editing root Canvas and before applying the zoom factor of the Canvas view navigation state
    Vector2 CalculateCanvasViewExtent(UINT viewportWidth, UINT viewportHeight) const;

    // Helper funtion to apply the user manipulation in Canvas View to the viewport of Canvas View
    void ApplyCanvasNavigationInput(const CanvasNavigationInput& input, UINT viewportWidth, UINT viewportHeight);

    void ApplySceneNavigationInput(const SceneNavigationInput& input, float deltaTime);
    std::optional<EditorCameraFocusBounds> BuildSelectedActorFocusBounds(SceneBase& scene);

    // Stop all ongoing edit transactions (transform, RectTransform, etc.)
    void StopAllEditTransactions();

	bool HandleWindowCloseRequest();
	void CompleteExitRequest();
	bool m_openOperationDiagnosticPopup = false;
	std::string m_operationDiagnostic;
};
