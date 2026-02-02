#include "TitleScene.h"
#include "Renderer.h"
#include "InputManager.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "EventManager.h"
#include "EventType.h"
#include "Audio.h"

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

// Initialization
void TitleScene::InitializeOverride(EngineContext& context)
{
}

// Update
void TitleScene::UpdateOverride()
{
}

// Finalize
void TitleScene::FinalizeOverride()
{
}
