#include "App.h"
#include <objbase.h>

//ちゃちのちゃわいいぷろぐらみんぐ


int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	//アプリケーションクラスのインスタンスを取得
	App* app = App::GetInstance();

	//COMライブラリの初期化
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	if (FAILED(hr))
	{
		return -1;
	}

	//初期化
	if (!app->Initialize())
	{
		return -1;
	}

	//実行
	app->Run();

	//終了
	app->Terminate();

	//COMライブラリの終了
	CoUninitialize();

	return 0;
}