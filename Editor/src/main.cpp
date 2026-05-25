#include "EditorApp.h"
#include "Engine/Core/Path/PathManager.h"
#include <objbase.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // Get executable path and initialize path manager
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if (!PathManager::Initialize(exePath)) return -1;

	// Initialize COM library for multithreaded use
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return -1;

    // Get singleton instance of editor application class
    EditorApp* app = EditorApp::GetInstance();

    // Initialize editor application
    if (!app->Initialize()) return -1;

    // Run editor application
    app->Run();

    // Terminate editor application
	app->Terminate();

    // Uninitialize COM library
    CoUninitialize();
	return 0;
}