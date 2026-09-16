#include "GameUIManager.h"
#include "GameManager.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/UI/UIImage.h"
#include <algorithm>
#include <cmath>
#include <iterator>

namespace
{
	UIImage* ResolveImage(SceneBase& scene, const ActorReference& reference)
	{
		Actor* actor = reference.Resolve(scene);
		return actor ? actor->GetComponentByClass<UIImage>() : nullptr;
	}

	void SetDigit(UIImage& image, int digit)
	{
		const Vector2 uvScale{ 0.2f, 0.5f };
		const Vector2 uvOffset{
			static_cast<float>(digit % 5) * uvScale.x,
			static_cast<float>(digit / 5) * uvScale.y
		};

		const Vector2 currentScale = image.GetUVScale();
		if (currentScale.x != uvScale.x || currentScale.y != uvScale.y)
		{
			image.SetUVScale(uvScale);
		}

		const Vector2 currentOffset = image.GetUVOffset();
		if (currentOffset.x != uvOffset.x || currentOffset.y != uvOffset.y)
		{
			image.SetUVOffset(uvOffset);
		}
	}
}

REGISTER_GAME_COMPONENT(GameUIManager)

std::optional<TypeMetadata> GameUIManager::BuildMetadata()
{
    TypeMetadataBuilder<GameUIManager> builder("GameUIManager");

    /*---- Describe the property registration process ----*/
    // Example: register a private member declared in GameUIManager.
    // builder.Property("speed", &GameUIManager::m_speed);

	builder.Property("GameManagerActor", &GameUIManager::m_gameManagerActor).Optional();
	builder.Property("Digit100sActor", &GameUIManager::m_digit100sActor).Optional();
	builder.Property("Digit10sActor", &GameUIManager::m_digit10sActor).Optional();
	builder.Property("Digit1sActor", &GameUIManager::m_digit1sActor).Optional();
	builder.Property("Digit01sActor", &GameUIManager::m_digit01sActor).Optional();
	builder.Property("Digit001sActor", &GameUIManager::m_digit001sActor).Optional();

    return builder.Build();
}

void GameUIManager::Start()
{
	LateUpdate(); // Display the initial remaining time before the first game update.
}
void GameUIManager::PreUpdate() {}
void GameUIManager::Update() {}
void GameUIManager::LateUpdate()
{
	Actor* owner = GetOwner();
	SceneBase* scene = owner ? owner->GetOwner() : nullptr;
	if (!scene) return;

	Actor* managerActor = m_gameManagerActor.Resolve(*scene);
	GameManager* manager = managerActor
		? managerActor->GetComponentByClass<GameManager>()
		: nullptr;
	if (!manager) return;

	UIImage* images[] = {
		ResolveImage(*scene, m_digit100sActor),
		ResolveImage(*scene, m_digit10sActor),
		ResolveImage(*scene, m_digit1sActor),
		ResolveImage(*scene, m_digit01sActor),
		ResolveImage(*scene, m_digit001sActor)
	};
	if (std::any_of(std::begin(images), std::end(images), [](UIImage* image) { return image == nullptr; }))
	{
		return;
	}

	const float restTime = manager->GetRestTime();
	if (!std::isfinite(restTime)) return;

	// Round up so that 000.00 is shown only when the timer has expired.
	// A small tolerance avoids advancing a digit at exact centiseconds stored as float.
	const double clampedTime = std::clamp(static_cast<double>(restTime), 0.0, 999.99);
	const int centiseconds = restTime <= 0.0f
		? 0
		: std::clamp(std::max(1, static_cast<int>(std::ceil(clampedTime * 100.0 - 0.005))), 1, 99999);
	const int digits[] = {
		centiseconds / 10000,
		centiseconds / 1000 % 10,
		centiseconds / 100 % 10,
		centiseconds / 10 % 10,
		centiseconds % 10
	};
	for (size_t i = 0; i < std::size(images); ++i)
	{
		SetDigit(*images[i], digits[i]);
	}
}
void GameUIManager::Destroy() {}
