#pragma once
#include <windows.h>
#include <memory>
#include <string>

#include "Engine/Engine.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"

#include "EditorCamera.h"
#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "MenuBar.h"
#include "ScriptsPanel.h"
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

	InputManager& m_inputManager = InputManager::GetInstance();
    TimeManager& m_timeManager = TimeManager::GetInstance();

    EngineContext m_engineContext;

    // The scene currently being edited
    std::unique_ptr<SceneBase> m_pScene;

    // Editor-only free-fly camera. Kept outside SceneBase so it is never
    // written to / read from .scene files.
    std::unique_ptr<Actor> m_pEditorCameraActor;
    EditorCamera* m_pEditorCamera = nullptr;

    // Panels
    HierarchyPanel m_hierarchyPanel;
    InspectorPanel m_inspectorPanel;
    MenuBar m_menuBar;
	ScriptsPanel m_scriptsPanel;

	HMODULE m_hGameCodeDll = nullptr;    // Handle to the loaded game code DLL (for hot-reloading)

private:
    EditorApp() = default;

    void CreateMainWindow();
    void PrepareInstance();
    void InitInstance();
    void InitImGui();

    void Update(float deltaTime);
    void Render();
    void RenderImGui();
    void ShutdownImGui();
};
