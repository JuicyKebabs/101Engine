#pragma once
#include <windows.h>
#include <memory>
#include <optional>
#include <string>

#include "Engine/Engine.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Window/Window.h"
#include "Command/EditorCommandHistory.h"
#include "Command/RectTransformEditCommand.h"
#include "Core/CanvasEditContext.h"

#include "Core/EditorViewCamera.h"
#include "UI/HierarchyPanel.h"
#include "UI/Inspector/InspectorPanel.h"
#include "UI/SceneViewPanel.h"
#include "UI/MenuBar.h"
#include "UI/Toolbar.h"
#include "UI/ScriptsPanel.h"

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

// Struct to hold the navigation state of the Canvas view in the editor
// Used to reflect the user manipulation to the Canvas view (panning, zooming, etc.)
// User manipulation -> Store the result as CanvasViewNavigation -> Change the Canvas view to reflect the navigation state
struct CanvasViewNavigation
{
	Vector2 center = Vector2::Zero();	// Camara center position in the Canvas view (in Editing-Root Canvas local space)
	float zoom = 1.0f;					// Zoom level of the Canvas view (Fitting the Canvas rectangle to the viewport is 1.0f)
	Guid canvasActorGuid;				// Guid of the editing root Canvas Actor 
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
    void Run();
    void Terminate();

private:
    enum class EditorModeTransition
    {
        None,
        EnterPlay,
        ExitPlay
    };

    Window m_window;

    std::unique_ptr<Engine> m_pEngine;
    std::unique_ptr<Renderer> m_pRenderer;
    std::unique_ptr<TextureManager> m_pTextureManager;
    std::unique_ptr<MeshManager> m_pMeshManager;
    std::unique_ptr<AssetManager> m_pAssetManager;

    InputManager& m_inputManager = InputManager::GetInstance();
    TimeManager& m_timeManager = TimeManager::GetInstance();

    EngineContext m_engineContext;

    EditorMode m_editorMode = EditorMode::Edit; // The current mode of the editor (Edit or Play)
    EditorModeTransition m_pendingModeTransition = EditorModeTransition::None;
    std::unique_ptr<SceneBase> m_pEditScene;    // The scene currently being edited
    std::unique_ptr<SceneBase> m_pPlayScene;    // The scene currently being played (runtime)

    EditorCommandHistory m_commandHistory;  // Command history for undo/redo

    // Camera facade owned by the Scene View and kept outside SceneBase so it
    // is never written to / read from .scene files.
    EditorViewCamera m_editorViewCamera;

    // Canvas View controls
    CanvasEditContext m_canvasEditContext;          // Stores the information for editing a Canvas in the Canvas view mode.
    CanvasViewNavigation m_canvasViewNavigation;    // Stores the information of manipulation on the Canvas View

    // Panels
    HierarchyPanel m_hierarchyPanel;
    InspectorPanel m_inspectorPanel;
    SceneViewPanel m_sceneViewPanel;
    MenuBar m_menuBar;
    Toolbar m_toolbar;
    ScriptsPanel m_scriptsPanel;

    // ImGui keeps IniFilename as a raw pointer, so the backing string must
    // remain alive for the entire ImGui context lifetime.
    std::string m_imguiIniPath;
    bool m_shouldBuildDefaultDockLayout = false;

    HMODULE m_hGameCodeDll = nullptr;    // Handle to the loaded game code DLL (for hot-reloading)

    // Render data for rendering the outline of a selected object in the scene view
    FrameRenderData m_selectionRenderData;

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
    void ReloadGameCode(bool reconfigure);          // For hot-reloading game code DLL
    void DeleteScript(const std::string& name);     // Delete a script file from the project
    void EnterPlayMode();                           // Switch to Play mode
    void ExitPlayMode();                            // Switch back to Edit mode
    void ApplyPendingModeTransition();              // Apply a deferred Play/Edit mode transition

    bool SaveHotReloadSnapshot();
    bool BuildStagedGameCode(bool reconfigure);
    bool DestroyCurrentRuntimeState();
    bool PromoteStagedGameCode();
    bool RestorePreviousGameCode();
    bool RollbackGameCode();
    bool RestoreHotReloadSnapshot();
    void RemovePreviousGameCodeBackup();

    void PrepareInstance();
    void InitInstance();
    void InitImGui();
    void RegisterComponentInspectors();

    void Update(float deltaTime);
    void Render();

    void RenderEditViewport(SceneBase* activeScene, GpuTexture* sceneColor);
    void RenderPlayViewport(SceneBase* activeScene, GpuTexture* sceneColor);

    void RenderShadowPass();
    void RenderWorldPass();
    void RenderSelectionPass();

    void RenderImGui();
    void RenderMainDockSpace();
    void BuildDefaultDockLayout(unsigned int dockSpaceId);
    void RenderHierarchyPanel();
    void RenderInspectorPanel();
    void RenderSceneViewPanel();
    void RenderMenuBar();
	void RenderToolbar();
    void RenderScriptsPanel();
    void ShutdownImGui();

    SceneBase* GetActiveScene() const;

    void ApplySceneViewResizeRequest();
    void ApplySceneRenderTargetSizeToScene(SceneBase& scene);

    // Build render data for the selected object in the scene view
    // (used to render an outline around the selected object)
    void BuildSelectionRenderData(
        RenderSpace targetRenderSpace,
        const CameraInfo& viewportCameraInfo,
        const Canvas* canvasViewRoot
    );

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
};
