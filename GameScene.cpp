#include "GameScene.h"
#include "Renderer.h"
#include "InputManager.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "EventManager.h"
#include "EventType.h"
#include "Audio.h"
#include "Player.h"

using namespace DirectX;

// Constructor
GameScene::GameScene(float window_width, float window_height)
	: SceneBase(window_width, window_height)
{
}

// Destructor
GameScene::~GameScene()
{
}

// Initialization
void GameScene::InitializeOverride(EngineContext& engineContext)
{
	Actor* player = new Actor();
	player->AddComponent<PlayerBehavior>(player);
	AddObject(std::unique_ptr<Actor>(player));
}

// Update
void GameScene::UpdateOverride()
{
}

// Post-collision processing
void GameScene::ResolveCollisions()
{
}

// Finalize
void GameScene::FinalizeOverride()
{
}