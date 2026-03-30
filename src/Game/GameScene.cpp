#include "GameScene.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Game/Player.h"
#include "Game/CameraTest.h"

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

	auto cameraTestActor = AddActor<Actor>(Vector3{ 0.0f, 0.0f, -5.0f });
	cameraTestActor->AddComponent<CameraTest>();
	auto camera = cameraTestActor->AddComponent<Camera>(1980.0f, 1080.0f);
	//camera->SetRotationMode(CAMERA_ROTATION_MODE::ROTATION_MODE_LOOK_AT_TARGET);
	//camera->SetTargetActor(playerActor);
}