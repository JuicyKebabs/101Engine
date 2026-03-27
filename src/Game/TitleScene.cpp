#include "TitleScene.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Audio/Audio.h"

using namespace DirectX;

// Constructor
TitleScene::TitleScene(float window_width, float window_height)
	: SceneBase(window_width, window_height)
{
}

// Destructor
TitleScene::~TitleScene()
{
}