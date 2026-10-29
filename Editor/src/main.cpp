#include "Core/EditorApp.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Core/Path/ExecutablePath.h"
#include <objbase.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    const auto executablePath = GetExecutableDirectory() / L"101Editor.exe";

    if (!PathManager::Initialize(executablePath.string()))
    {
        MessageBoxW(nullptr, L"project.101 is not found", L"101Engine", MB_OK);
        return EXIT_FAILURE;
    }

    const HRESULT comStatus = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    if (FAILED(comStatus))
    {
        return EXIT_FAILURE;
    }

    int exitCode = EXIT_FAILURE;
    EditorApp* app = EditorApp::GetInstance();

    try
    {
        if (app->Initialize())
        {
            // Run owns the loop outcome; a renderer failure must survive the
            // common cleanup below instead of being overwritten with success.
            exitCode = app->Run();
        }
    }
    catch (const std::exception& error)
    {
        MessageBoxA(nullptr, error.what(), "101Engine", MB_OK);
    }

    // Partial initialization follows the same reverse ownership order.
    app->Terminate();
    CoUninitialize();
    return exitCode;
}
