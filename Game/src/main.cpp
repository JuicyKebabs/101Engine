#include "App/App.h"
#include "Engine/Core/Path/PathManager.h"
#include <objbase.h>

//‚¿‚á‚¿‚Ì‚¿‚á‚í‚¢‚¢‚Õ‚ë‚®‚ç‚Ý‚ñ‚®


int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// Get executable path and initialize path manager
	char exePath[MAX_PATH];
	GetModuleFileNameA(nullptr, exePath, MAX_PATH);
	if (!PathManager::Initialize(exePath))
	{
		MessageBoxA(nullptr, "project.101 is not found", "Error", MB_OK);
		return -1;
	}

	// Initialize COM library for multithreaded use
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	// If initialization failed, exit with error code
	if (FAILED(hr)) return -1;

	// Get singleton instance of application class
	App* app = App::GetInstance();

	// Initialize application
	if (!app->Initialize()) return -1;

	//===========================================================================
	// Initialize scene with test scene by json file for testing
	// This will be removed after data driven scene management is implemented
	//===========================================================================
	auto* sm = app->GetSceneManager();
	sm->RegisterSceneFile("test", "asset/scenes/test.scene");		// Register test scene
	sm->RegisterSceneFile("output", "asset/scenes/output.scene");	// Register test scene
	sm->SetInitialScene("test");									// Set initial scene
	app->InitSceneManager();										// Initialize scene manager

	// Run application
	app->Run();

	// Terminate application
	app->Terminate();

	// Uninitialize COM library
	CoUninitialize();

	return 0;
}