#include "GameScene.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
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

void GameScene::InitializeOverride(EngineContext& context)
{
	// Create player actor and add it to the scene
	auto playerActor = AddActor<Actor>(Vector3{ 0.0f, 0.0f, 5.0f });
	playerActor->AddComponent<PlayerBehavior>();
	playerActor->AddComponent<MeshRenderer>()->Initialize(
		RenderTemplateFactory::CreateRenderTemplateFromDefaultMesh(
			*context.pMeshManager,
			*context.pTextureManager,
			DEFAULT_MESH::CUBE,
			MaterialInput{.texturePath = L""}
		)
	);

	auto playerChild = playerActor->AddChild<Actor>(Vector3{ 0.0f, 0.0f, -1.0f });
	playerChild->AddComponent<MeshRenderer>()->Initialize(
		RenderTemplateFactory::CreateRenderTemplateFromDefaultMesh(
			*context.pMeshManager,
			*context.pTextureManager,
			DEFAULT_MESH::SPHERE,
			MaterialInput{.texturePath = L"asset/texture/skin.png",.baseColor = Vector4(1.0f, 1.0f, 1.0f, 0.3f), .psoKey = PSO_KEY_TRANSPARENT,}
		)
	);

	auto modelActor = AddActor<Actor>(Vector3{ 2.0f, 0.0f, 5.0f });
	modelActor->AddComponent<PlayerBehavior>();
	modelActor->AddComponent<MeshRenderer>()->Initialize(
		RenderTemplateFactory::CreateRenderTemplate(
			*context.pMeshManager,
			*context.pTextureManager,
			MeshInput{ .modelPath = L"asset/fbx/spray/Spray_01.fbx" },
			MaterialInput{.texturePath = L"asset/fbx/sourceimages/T_Spray01.png"}
		)
	);
}