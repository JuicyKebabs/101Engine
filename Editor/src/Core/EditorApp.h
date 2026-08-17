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
#include "Command/EditorCommandHistory.h"
#include "Core/CanvasEditContext.h"

#include "Core/EditorCamera.h"
#include "UI/HierarchyPanel.h"
#include "UI/Inspector/InspectorPanel.h"
#include "UI/MenuBar.h"
#include "UI/ScriptsPanel.h"
#include "UI/SceneViewPanel.h"

//--------------------------------------------
// EditorApp class
// The main application class for the editor.
//--------------------------------------------

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

	void NewScene();                                // Create a new scene with default settings
	void LoadScene(const std::string& filePath);    // Load a scene from a file
	void ReloadGameCode(bool reconfigure);          // For hot-reloading game code DLL
	void DeleteScript(const std::string& name);     // Delete a script file from the project

private:
    HWND m_hwnd = nullptr;
    WNDCLASSEX m_wc = {};

    std::unique_ptr<Engine> m_pEngine;
    std::unique_ptr<Renderer> m_pRenderer;
    std::unique_ptr<TextureManager> m_pTextureManager;
    std::unique_ptr<MeshManager> m_pMeshManager;
	std::unique_ptr<AssetManager> m_pAssetManager;

	InputManager& m_inputManager = InputManager::GetInstance();
    TimeManager& m_timeManager = TimeManager::GetInstance();

    EngineContext m_engineContext;

    // The scene currently being edited
    std::unique_ptr<SceneBase> m_pScene;

	EditorCommandHistory m_commandHistory;  // Command history for undo/redo

    // Editor-only free-fly camera. Kept outside SceneBase so it is never
    // written to / read from .scene files.
    std::unique_ptr<Actor> m_pEditorCameraActor;
    EditorCamera* m_pEditorCamera = nullptr;

    // Canvas View controls
	CanvasEditContext m_canvasEditContext;          // Stores the information for editing a Canvas in the Canvas view mode.
	CanvasViewNavigation m_canvasViewNavigation;    // Stores the information of manipulation on the Canvas View

    // Panels
    HierarchyPanel m_hierarchyPanel;
    InspectorPanel m_inspectorPanel;
    SceneViewPanel m_sceneViewPanel;
    MenuBar m_menuBar;
	ScriptsPanel m_scriptsPanel;

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

private:
    EditorApp() = default;

    void CreateMainWindow();
    void PrepareInstance();
    void InitInstance();
    void InitImGui();
    void RegisterComponentInspectors();

    void Update(float deltaTime);
    void Render();
    void RenderImGui();
    void RenderMenuBar();
    void RenderScriptsPanel();
    void RenderHierarchyPanel();
    void RenderInspectorPanel();
    void RenderSceneViewPanel();
    void ShutdownImGui();

	void ApplySceneViewResizeRequest();
    void ApplyCurrentViewportSizeToScene();

	// Helper functions for callbacks from the InspectorPanel to track transform edits for undo/redo
	void BeginTransformEdit(const Guid& actorGuid, const Transform3D& before);
	void EndTransformEdit(const Guid& actorGuid, const Transform3D& after);
    void CancelTransformEdit();

	// Build render data for the selected object in the scene view
    // (used to render an outline around the selected object)
    void BuildSelectionRenderData(
        RenderSpace targetRenderSpace,
        const CameraInfo& viewportCameraInfo,
        const Canvas* canvasViewRoot
    );

	// Helper function to build CameraInfo for the current viewport size
	// Camera matrix is built based on the current viewport mode (Scene or Canvas)
    CameraInfo BuildViewportCameraInfo(UINT viewportWidth, UINT viewportHeight) const;

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
};
