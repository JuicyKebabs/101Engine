#pragma once
#include <windows.h>
#include <memory>
#include "Engine/Engine.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Core/Context/Context.h"

class EditorApp
{
public:
	EditorApp(const EditorApp&) = delete;
	EditorApp& operator=(const EditorApp&) = delete;
	EditorApp(EditorApp&&) = delete;
	EditorApp& operator=(EditorApp&&) = delete;

	static EditorApp* GetInstance() {
		static EditorApp instance;
		return &instance;
	}

	bool Initialize();
	void Run();
	void Terminate();

private:
	HWND m_hwnd = nullptr;
	WNDCLASSEX m_wc = {};

	std::unique_ptr<Engine> m_pEngine;
	std::unique_ptr<Renderer> m_pRenderer;
	std::unique_ptr<TextureManager> m_pTextureManager;
	std::unique_ptr<MeshManager> m_pMeshManager;

	TimeManager& m_timeManager = TimeManager::GetInstance();
	InputManager& m_inputManager = InputManager::GetInstance();

	EngineContext m_engineContext;

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