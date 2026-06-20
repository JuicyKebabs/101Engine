#include "EditorApp.h"
#include "Engine/Core/Path/PathManager.h"
#include <objbase.h>
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if (!PathManager::Initialize(exePath)) return -1;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return -1;

    EditorApp* app = EditorApp::GetInstance();
    if (!app->Initialize()) return -1;
    app->Run();
    app->Terminate();

    CoUninitialize();
    return 0;
}
