#include "GameScene.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Game/Player.h"
#include "Game/CameraTest.h"
#include "Engine/Component/Collider.h"
#include "Engine/Actor/ActorFactory.h"

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
	m_directionalLight.position = Vector3{ 0.0f, 0.0f, 0.0f };
	m_directionalLight.color = Vector3{ 1.0f, 1.0f, 1.0f };

    auto playerActor = AddActor(ActorFactory::CreateActor(ActorType::Mesh, Actor::InitDesc()));
	playerActor->GetComponentByClass<Transform>()->Init(Transform::InitDesc(Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 1.0f, 1.0f, 1.0f }));
	auto playerMesh = playerActor->GetComponentByClass<MeshRenderer>();
	playerMesh->Init(
		RenderTemplateFactory::CreateMeshRenderTemplateFromDefaultMesh(
			*context.pMeshManager,
			*context.pTextureManager,
			DEFAULT_MESH::SPHERE,
			MaterialInput{ .texturePath = L"asset/texture/skin.png",.baseColor = Vector4(1.0f, 1.0f, 1.0f, 0.3f), .psoKey = PSO_KEY_DEFAULT::MESH_OPAQUE.WithLighting(), }
		)
	);

	Collider::InitDesc playerColliderDesc;
	playerColliderDesc.type = ColliderType::BOX;
	playerColliderDesc.localScale = Vector3{ 1.0f, 1.0f, 1.0f };
	playerActor->AddComponent<Collider>()->Init(playerColliderDesc);


    auto modelActor = AddActor(ActorFactory::CreateActor(ActorType::Mesh, Actor::InitDesc()));
	modelActor->GetComponentByClass<Transform>()->Init(Transform::InitDesc(Vector3{5.0f, 0.0f, 0.0f}, Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 1.0f, 1.0f, 1.0f }));
	modelActor->AddComponent<PlayerBehavior>();
	auto modelMesh = modelActor->GetComponentByClass<MeshRenderer>();
	modelMesh->Init(
		RenderTemplateFactory::CreateMeshRenderTemplateFromDefaultMesh(
			*context.pMeshManager,
			*context.pTextureManager,
			DEFAULT_MESH::SPHERE,
			MaterialInput{ .texturePath = L"asset/texture/skin.png",.psoKey = PSO_KEY_DEFAULT::MESH_OPAQUE.WithLighting(), }
		)
	);
	modelActor->AddComponent<Collider>()->Init(playerColliderDesc);


    auto groundActor = AddActor(ActorFactory::CreateActor(ActorType::Mesh, Actor::InitDesc()));
	groundActor->GetComponentByClass<Transform>()->Init(Transform::InitDesc(Vector3{ 0.0f, -10.0f, 0.0f }, Vector3{ 50.0f, 0.0f, 50.0f }, Vector3{ 1.0f, 1.0f, 1.0f }));
	auto groundMesh = groundActor->GetComponentByClass<MeshRenderer>();
	groundMesh->Init(
		RenderTemplateFactory::CreateMeshRenderTemplateFromDefaultMesh(
			*context.pMeshManager,
			*context.pTextureManager,
			DEFAULT_MESH::CUBE,
			MaterialInput{ .texturePath = L"asset/texture/white.png",.psoKey = PSO_KEY_DEFAULT::MESH_OPAQUE.WithLighting(), }
		)
	);

	Collider::InitDesc groundColliderDesc;
	groundColliderDesc.type = ColliderType::BOX;
	groundColliderDesc.localScale = Vector3{ 1.0f, 1.0f, 1.0f };
	groundActor->AddComponent<Collider>()->Init(groundColliderDesc);

	auto cameraTestActor = AddActor(ActorFactory::CreateActor(ActorType::Camera, Actor::InitDesc()));
	auto camera = cameraTestActor->AddComponent<Camera>();
	camera->Init(Camera::InitDesc(1920.0f, 1080.0f));
	//camera->SetRotationMode(CAMERA_ROTATION_MODE::ROTATION_MODE_LOOK_AT_TARGET);
	//camera->SetTargetActor(playerActor);
}