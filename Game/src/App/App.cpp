#include "App.h"

#include <cstdlib>
#include <mmsystem.h>
#include "Renderer/Win32WindowOutputDesc.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Core/Path/ExecutablePath.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Project/ProjectSettings.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Scene/ComponentRegistry.h"

#pragma comment(lib, "winmm.lib")

App* App::GetInstance()
{
    static App instance;
    return &instance;
}

bool App::CheckRenderingResult(const rendering::Result& result)
{
    if (result.IsSuccess())
    {
        return true;
    }

    DBG("Game Renderer: %s", result.message.c_str());

    if (result.failureKind == rendering::FailureKind::GpuCompletionUnknown)
    {
        // No destructor may release resources whose GPU completion is unknown.
        // OS process teardown is the only safe final owner in this fatal case.
        ExitProcess(EXIT_FAILURE);
    }

    // Once this session fails, a later WM_QUIT must not turn it into success.
    // Known-completion failures unwind normally; the fatal path above does not.
    m_renderingStopped = true;
    m_running = false;
    return false;
}

bool App::Initialize()
{
    const auto executableDirectory = GetExecutableDirectory();
    m_gameCode = LoadLibraryW((executableDirectory / L"GameCode.dll").c_str());

    if (!m_gameCode)
    {
        DBG("GameCode.dll could not be loaded.");
        return false;
    }

    Window::InitDesc windowDescription{};
    windowDescription.className = L"101EngineGameWindow";
    windowDescription.title = L"101Engine";
    windowDescription.clientWidth = WINDOW_WIDTH;
    windowDescription.clientHeight = WINDOW_HEIGHT;
    windowDescription.allowFullscreenToggle = true;
    windowDescription.messageCallback = [](HWND window, UINT message, WPARAM key, LPARAM parameter, LRESULT&)
    {
        InputManager::GetInstance().ProcessWindowMessage(message, key, parameter);

        if (message == WM_KEYDOWN && key == VK_ESCAPE)
        {
            SendMessageW(window, WM_CLOSE, 0, 0);
        }

        return false;
    };

    if (!m_window.Initialize(windowDescription))
    {
        return false;
    }

    m_renderer = std::make_unique<rendering::Renderer>();
    rendering::RendererConfiguration rendererConfiguration{};
#ifdef _DEBUG
    rendererConfiguration.enableDebugLayer = true;
#endif

    if (!CheckRenderingResult(m_renderer->Initialize(rendererConfiguration)))
    {
        return false;
    }

    rendering::Win32WindowOutputDesc windowOutputDescription{};
    windowOutputDescription.window = m_window.GetHandle();
    windowOutputDescription.width = WINDOW_WIDTH;
    windowOutputDescription.height = WINDOW_HEIGHT;
    auto outputCreation = m_renderer->CreateWindowOutput(windowOutputDescription);

    if (!CheckRenderingResult(outputCreation.status))
    {
        return false;
    }

    m_windowOutput = outputCreation.handle;
    m_renderAssets = std::make_unique<RenderAssetResources>();

    if (!m_renderAssets->Initialize(*m_renderer, executableDirectory / L"shaders"))
    {
        return false;
    }

    // Audio absence is recoverable; visual assets still require Renderer setup.
    if (!m_audio.Initialize())
    {
        DBG("Game: audio unavailable; continuing without audio.");
    }

    m_assets = std::make_unique<AssetManager>();

    if (!m_assets->Initialize(PathManager::Resolve("asset"), m_renderer.get(), m_renderAssets.get(), &m_audio))
    {
        return false;
    }

    m_imprints = std::make_unique<ActorImprintSystem>(*m_assets);
    m_scenes = std::make_unique<SceneManager>();
    m_context.pRenderer = m_renderer.get();
    m_context.pRenderAssets = m_renderAssets.get();
    m_context.pAssetManager = m_assets.get();
    m_context.pActorImprintSystem = m_imprints.get();
    m_context.pAudioManager = &m_audio;
    m_input.Initialize();

    if (!InitSceneManager())
    {
        return false;
    }

    RAWINPUTDEVICE mouseDevice{};
    mouseDevice.usUsagePage = 0x01;
    mouseDevice.usUsage = 0x02;
    mouseDevice.hwndTarget = m_window.GetHandle();
    m_input.SetRawMouseLookEnabled(RegisterRawInputDevices(&mouseDevice, 1, sizeof(mouseDevice)) != FALSE);
    return true;
}

bool App::InitSceneManager()
{
    ProjectSettings settings;
    std::string error;

    if (!ProjectSettings::Load(PathManager::Resolve("project.101"), settings, &error))
    {
        DBG("Game project settings: %s", error.c_str());
        return false;
    }

    for (const auto& tag : settings.GetUserTags())
    {
        if (!TagRegistry::Get().RegisterUserTag(tag, nullptr, &error))
        {
            DBG("Game project tag: %s", error.c_str());
            return false;
        }
    }

    if (!m_scenes->SetInitialScene(settings.GetGameStartupSceneGuid()))
    {
        DBG("Game startup Scene GUID is missing.");
        return false;
    }

    m_scenes->SetViewportSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    m_scenes->Initialize(m_context);
    return m_scenes->GetCurrentScene() != nullptr;
}

int App::Run()
{
    m_window.Show();
    timeBeginPeriod(1);
    MSG message{};
    DWORD lastFrame = timeGetTime();

    while (m_running && message.message != WM_QUIT)
    {
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            continue;
        }

        const DWORD now = timeGetTime();

        if (now - lastFrame < 16)
        {
            Sleep(1);
            continue;
        }

        lastFrame = now;

        if (!m_window.ApplyPendingModeChange())
        {
            DBG("Game window mode change failed.");
        }

        if (m_window.IsMinimized())
        {
            m_time.Update();
            continue;
        }

        if (!ApplyWindowResizeRequest())
        {
            continue;
        }

        Update();
        Render();
        m_input.Copy();
    }

    timeEndPeriod(1);

    if (m_renderingStopped)
    {
        return EXIT_FAILURE;
    }

    return static_cast<int>(message.wParam);
}

bool App::ApplyWindowResizeRequest()
{
    Window::Size requestedSize{};

    if (!m_window.GetResizeRequest(requestedSize))
    {
        return true;
    }

    if (!CheckRenderingResult(m_renderer->ResizeWindowOutput(m_windowOutput, requestedSize.width, requestedSize.height)))
    {
        return false;
    }

    m_scenes->SetViewportSize(requestedSize.width, requestedSize.height);
    m_window.CommitResize();
    return true;
}

void App::Update()
{
    m_time.Update();
    const float deltaTime = m_time.GetDeltaTime();
    m_input.Update();
    m_audio.Update();
    m_scenes->PreUpdate(deltaTime);
    m_scenes->Update(deltaTime);
    m_scenes->LateUpdate(deltaTime);
}

void App::Render()
{
    rendering::FrameRenderData frame{};
    frame.time = static_cast<float>(timeGetTime()) / 1000.0f;
    rendering::ViewOutput output = m_windowOutput;

    if (SceneBase* scene = m_scenes->GetCurrentScene())
    {
        if (!scene->AppendRenderData(frame, output))
        {
            return;
        }
    }
    else
    {
        // A valid frame without Camera/Scene must still clear/present the host.
        rendering::RenderView background{};
        background.id = 2;
        background.output = output;
        background.kind = rendering::ViewKind::Overlay;
        frame.views.push_back(background);
    }

    if (!CheckRenderingResult(m_renderer->SubmitFrameRenderData(std::move(frame))))
    {
        return;
    }

    const auto result = m_renderer->Render();
    CheckRenderingResult(result.status);

    for (const auto& diagnostic : result.diagnostics)
    {
        DBG("Game draw: %s", diagnostic.message.c_str());
    }
}

void App::Terminate()
{
    // Scenes/Components go first while their AssetManager and Renderer are live.
    if (m_scenes)
    {
        m_scenes->Finalize();
        m_scenes.reset();
    }

    m_imprints.reset();
    m_context.pActorImprintSystem = nullptr;
    m_assets.reset();
    m_context.pAssetManager = nullptr;
    ComponentRegistry::Get().UnregisterAllGameComponents();

    if (m_gameCode)
    {
        FreeLibrary(m_gameCode);
        m_gameCode = nullptr;
    }

    m_audio.Terminate();
    m_context.pAudioManager = nullptr;
    m_renderAssets.reset();
    m_context.pRenderAssets = nullptr;
    m_renderer.reset();
    m_context.pRenderer = nullptr;
    m_window.Terminate();
}
