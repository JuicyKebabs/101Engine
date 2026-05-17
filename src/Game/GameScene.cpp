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
#include "Engine/Component/RectTransform.h"
#include "Engine/UI/UIImage.h"
#include "Engine/Actor/ActorFactory.h"

using namespace DirectX;

// Constructor
GameScene::GameScene() : SceneBase()
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

 //   auto playerActor = AddActor(ActorFactory::CreateActor(ActorType::Mesh, Actor::ParamDesc()));
	//playerActor->GetComponentByClass<Transform>()->Init(Transform::ParamDesc(Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 1.0f, 1.0f, 1.0f }));
	//auto playerMesh = playerActor->GetComponentByClass<MeshRenderer>();
	//playerMesh->Init(
	//	RenderTemplateFactory::CreateMeshRenderTemplateFromDefaultMesh(
	//		*context.pMeshManager,
	//		*context.pTextureManager,
	//		DEFAULT_MESH::SPHERE,
	//		MaterialInput{ .texturePath = L"asset/texture/skin.png",.baseColor = Vector4(1.0f, 1.0f, 1.0f, 0.3f), .psoKey = PSO_KEY_DEFAULT::MESH_OPAQUE.WithLighting(), }
	//	)
	//);

	//Collider::ParamDesc playerColliderDesc;
	//playerColliderDesc.type = ColliderType::BOX;
	//playerColliderDesc.localScale = Vector3{ 1.0f, 1.0f, 1.0f };
	//playerActor->AddComponent<Collider>()->Init(playerColliderDesc);


 //   auto modelActor = AddActor(ActorFactory::CreateActor(ActorType::Mesh, Actor::ParamDesc()));
	//modelActor->GetComponentByClass<Transform>()->Init(Transform::ParamDesc(Vector3{5.0f, 0.0f, 0.0f}, Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 1.0f, 1.0f, 1.0f }));
	//modelActor->AddComponent<PlayerBehavior>()->Init();
	//auto modelMesh = modelActor->GetComponentByClass<MeshRenderer>();
	//modelMesh->Init(
	//	RenderTemplateFactory::CreateMeshRenderTemplateFromDefaultMesh(
	//		*context.pMeshManager,
	//		*context.pTextureManager,
	//		DEFAULT_MESH::SPHERE,
	//		MaterialInput{ .texturePath = L"asset/texture/skin.png",.psoKey = PSO_KEY_DEFAULT::MESH_OPAQUE.WithLighting(), }
	//	)
	//);
	//modelActor->AddComponent<Collider>()->Init(playerColliderDesc);


 //   auto groundActor = AddActor(ActorFactory::CreateActor(ActorType::Mesh, Actor::ParamDesc()));
	//groundActor->GetComponentByClass<Transform>()->Init(Transform::ParamDesc(Vector3{ 0.0f, -5.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 50.0f, 1.0f, 50.0f }));
	//auto groundMesh = groundActor->GetComponentByClass<MeshRenderer>();
	//groundMesh->Init(
	//	RenderTemplateFactory::CreateMeshRenderTemplateFromDefaultMesh(
	//		*context.pMeshManager,
	//		*context.pTextureManager,
	//		DEFAULT_MESH::CUBE,
	//		MaterialInput{ .texturePath = L"asset/texture/white.png",.psoKey = PSO_KEY_DEFAULT::MESH_OPAQUE.WithLighting(), }
	//	)
	//);

	//Collider::ParamDesc groundColliderDesc;
	//groundColliderDesc.type = ColliderType::BOX;
	//groundColliderDesc.localScale = Vector3{ 1.0f, 1.0f, 1.0f };
	//groundActor->AddComponent<Collider>()->Init(groundColliderDesc);





	
	auto canvasActor = AddActor(ActorFactory::CreateActor(ActorType::Canvas, Actor::InitDesc()));
	canvasActor->GetComponentByClass<RectTransform>()->SetParams(RectTransform::ParamDesc{.size = Vector2(1920, 1080)});
	canvasActor->GetComponentByClass<Canvas>()->Init(Canvas::ParamDesc());

	auto uiActor = AddActor(ActorFactory::CreateActor(ActorType::UI, Actor::InitDesc()));
	uiActor->GetComponentByClass<RectTransform>()->SetParams(RectTransform::ParamDesc{.anchorMode = AnchorMode::TopLeft,  .pivot = Vector2(0.0f, 1.0f), .size = Vector2(100, 100),});
	uiActor->GetComponentByClass<UIImage>()->SetParams(
		UIImage::ParamDesc{
			.pCanvas = canvasActor->GetComponentByClass<Canvas>(),
			.renderTemplate = RenderTemplateFactory::CreateUIImageRenderTemplate(
				*context.pTextureManager,
				MaterialInput{
					.texturePath = L"asset/texture/skin.png",
					.psoKey = PSO_KEY_DEFAULT::SPRITE_TRANSPARENT
				}),
			.order = 0,
			.color = Vector4(1, 1, 1, 1),
			.uvScale = Vector2(1, 1),
			.uvOffset = Vector2(0, 0),
			.flipX = false,
			.flipY = false
		}
	);
	uiActor->SetParent(canvasActor);

	auto cameraTestActor = AddActor(ActorFactory::CreateActor(ActorType::Camera, Actor::InitDesc()));
	cameraTestActor->GetComponentByClass<Camera>()->SetParams(Camera::ParamDesc{.window_width = 1920, .window_height = 1080});
	cameraTestActor->GetComponentByClass<Transform>()->SetParams(Transform::ParamDesc(Vector3(0,0,-6)));
	cameraTestActor->AddComponent<CameraTest>();
	//camera->SetRotationMode(CAMERA_ROTATION_MODE::ROTATION_MODE_LOOK_AT_TARGET);
	//camera->SetTargetActor(playerActor);
}