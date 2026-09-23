#include "FixedTarget.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/Time/Time.h"

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

	auto transform = owner ? owner->GetComponentByClass<Transform>() : nullptr;
	if (transform)
	{
		baseY = transform->GetLocalPosition().y;
	}

	// 経過時間をランダムに進める
	phase = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 10.0f; // 0から10秒の範囲でランダムに初期化
}
void FixedTarget::PreUpdate() {}
void FixedTarget::Update() 
{
	auto owner = GetOwner();
	auto transform = owner ? owner->GetComponentByClass<Transform>() : nullptr;

	if (transform)
	{
		const float speed = 1.0f; // 上下の動きの速さ
		const float amplitude = 0.3f; // 上下の動きの振幅
		float deltaTime = Time::GetTimeSeconds();

		float s = sinf(phase);

		float phaseSpeed = speed;

		if (s < 0.0f)
		{
			phaseSpeed = speed * 1.1f;
		}

		float wave = sin(phase);

		// -1 に近いほど速度を上げる
		float bottomFactor = std::max(0.0f, -wave);

		float currentSpeed = phaseSpeed * (1.0f + bottomFactor * 1.5f);
		phase += deltaTime * currentSpeed;

		float newY = baseY + amplitude * sinf(phase);

		// フワフワと浮かせるような動きをさせる
		Vector3 position = transform->GetLocalPosition();
		position.y = newY;
		transform->SetLocalPosition(position);
	}
}
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