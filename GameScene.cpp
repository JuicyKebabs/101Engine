#include "GameScene.h"
#include "Renderer.h"
#include "InputManager.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "EventManager.h"
#include "EventType.h"
#include "Audio.h"
#include "Player.h"
#include "Block.h"

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
	Player* pPlayer = new Player(MESH_TYPE::IMPORT);
	pPlayer->SetPosition({ 0.0f, 0.0f, 0.0f });
	pPlayer->SetScale({ 1.0f, 1.0f, 1.0f });
	AddObject(std::unique_ptr<Player>(pPlayer));

	Block* pBlock = new Block(MESH_TYPE::CUBE);
	pBlock->SetPosition({ 0.0f, -1.0f, 0.0f });
	pBlock->SetScale({ 10.0f, 10.0f, 10.0f });
	AddObject(std::unique_ptr<Block>(pBlock));

	m_pCamera->SetTarget({0.0f, 0.0f, 0.0f});
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