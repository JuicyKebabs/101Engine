#include "FixedTarget.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"

REGISTER_GAME_COMPONENT(FixedTarget)

std::optional<TypeMetadata> FixedTarget::BuildMetadata()
{
    TypeMetadataBuilder<FixedTarget> builder("FixedTarget");

    /*---- Describe the property registration process ----*/
    // Example: register a private member declared in FixedTarget.
    // builder.Property("speed", &FixedTarget::m_speed);
	builder.Property("explosionImprintRef", &FixedTarget::m_explosionImprintRef).Optional();
    return builder.Build();
}

void FixedTarget::Start()
{
	auto owner = GetOwner();
	auto scene = owner ? owner->GetOwner() : nullptr;
	auto sceneActors = scene ? scene->GetAllActors() : std::vector<Actor*>{};

	GameManager* gameManager = nullptr;

	for (auto actor : sceneActors)
	{
		if (actor)
		{
			gameManager = actor->GetComponentByClass<GameManager>();
			if (gameManager)
			{
				break;
			}
		}
	}

	m_gameManager = gameManager;
}
void FixedTarget::PreUpdate() {}
void FixedTarget::Update() {}
void FixedTarget::LateUpdate() {}
void FixedTarget::Destroy() {}

void  FixedTarget::Break()
{
	if (m_gameManager)
	{
		// ゲームマネージャーの残りターゲット数を減らす関数を呼び出す
		m_gameManager->DestroyTarget();
		auto system = GetEngineContext()->pActorImprintSystem;
		if (system)
		{
			ActorImprintHandle handle = system->Load(m_explosionImprintRef);
			Actor* explosion = system->Instantiate(*GetOwner()->GetOwner(), handle);
			Transform* transform = explosion ? explosion->GetComponentByClass<Transform>() : nullptr;
			if (transform)
			{
				transform->SetLocalPosition(GetOwner()->GetComponentByClass<Transform>()->GetLocalPosition());
			}
		}

	}
}