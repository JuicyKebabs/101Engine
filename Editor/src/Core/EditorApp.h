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
    void BuildSelectionRenderData(RenderSpace targetRenderSpace, const CameraInfo& viewportCameraInfo);

	// Helper function to build CameraInfo for the current viewport size
	// Camera matrix is built based on the current viewport mode (Scene or Screen)
    CameraInfo BuildViewportCameraInfo(UINT viewportWidth, UINT viewportHeight) const;
};
