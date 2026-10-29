#pragma once

#include <Windows.h>
#include <memory>
#include "Renderer/Renderer.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/RenderAssetResources.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Window/Window.h"

// Host owns the Window and Renderer instance. All native graphics objects are
// created and released inside Renderer; Scenes only keep typed registrations.
class App
{
public:
    static constexpr int WINDOW_WIDTH = 1920;
    static constexpr int WINDOW_HEIGHT = 1080;
    static App* GetInstance();
    bool Initialize();
    int Run();
    void Terminate();
    ~App() = default;

private:
    App() = default;
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    bool ApplyWindowResizeRequest();
    bool InitSceneManager();
    void Update();
    void Render();
    bool CheckRenderingResult(const rendering::Result& result);

    Window m_window;
    HMODULE m_gameCode = nullptr;
    std::unique_ptr<rendering::Renderer> m_renderer;
    rendering::WindowOutputHandle m_windowOutput;
    std::unique_ptr<RenderAssetResources> m_renderAssets;
    std::unique_ptr<AssetManager> m_assets;
    std::unique_ptr<ActorImprintSystem> m_imprints;
    std::unique_ptr<SceneManager> m_scenes;
    EngineContext m_context{};
    bool m_running = true;
    // Keep a renderer failure separate from a normal quit message so Run's
    // outcome survives the common reverse-order cleanup in main.
    bool m_renderingStopped = false;
    TimeManager& m_time = TimeManager::GetInstance();
    InputManager& m_input = InputManager::GetInstance();
    AudioManager& m_audio = AudioManager::GetInstance();
};
