#include "GameManager.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/UI/Canvas.h"
#include "Engine/UI/UIImage.h"

namespace
{
	bool SetUICanvasVisible(Actor& actor, bool visible)
	{
		Canvas* canvas = actor.GetComponentByClass<Canvas>();
		if (!canvas) return false;

		actor.SetActive(visible);
		canvas->SetVisible(visible);
		return true;
	}

	void SetUIImageVisible(SceneBase& scene, const ActorReference& reference,
		bool visible, const char* name)
	{
		Actor* actor = reference.Resolve(scene);
		if (!actor)
		{
			DBG("GameManager: %s is not found.", name);
			return;
		}

		UIImage* image = actor->GetComponentByClass<UIImage>();
		if (!image)
		{
			DBG("GameManager: %s has no UIImage.", name);
			return;
		}

		image->SetVisible(visible);
	}
}

REGISTER_GAME_COMPONENT(GameManager)

std::optional<TypeMetadata> GameManager::BuildMetadata()
{
    TypeMetadataBuilder<GameManager> builder("GameManager");

    /*---- Describe the property registration process ----*/
    // Example: register a private member declared in GameManager.
    // builder.Property("speed", &GameManager::m_speed);

	builder.Property("restTime", &GameManager::m_restTime).Optional();
	builder.Property("restTarget", &GameManager::m_restTarget).Optional();
	builder.Property("nextScene", &GameManager::m_nextScene).Optional();
	builder.Property("tutorialUIActor", &GameManager::m_tutorialUIActor).Optional();
	builder.Property("resultUIActor", &GameManager::m_resultUIActor).Optional();
	builder.Property("timerUIActor", &GameManager::m_timerUIActor).Optional();
	builder.Property("clearImageActor", &GameManager::m_clearImageActor).Optional();
	builder.Property("failedImageActor", &GameManager::m_failedImageActor).Optional();
	builder.Property("titlePromptImageActor", &GameManager::m_titlePromptImageActor).Optional();

    return builder.Build();
}

void GameManager::Start() 
{
	auto owner = GetOwner();
	auto scene = owner ? owner->GetOwner() : nullptr;
	auto sceneActors = scene ? scene->GetAllActors() : std::vector<Actor*>{};

	Player* player = nullptr;

	for (auto actor : sceneActors)
	{
		if (actor)
		{
			player = actor->GetComponentByClass<Player>();
			if (player)
			{
				break;
			}
		}
	}

	m_player = player;

	if (m_player)
	{
		m_player->SetCanMove(false); // プレイヤーの移動を禁止
	}
	else
	{
		DBG("GameManager: Player is not found.");
	}

	if (!scene)
	{
		DBG("GameManager: Scene is not found.");
		return;
	}

	auto tutorialUIActorPtr = m_tutorialUIActor.Resolve(*scene);
	auto resultUIActorPtr = m_resultUIActor.Resolve(*scene);

	if (tutorialUIActorPtr)
	{
		if (!SetUICanvasVisible(*tutorialUIActorPtr, true))
			DBG("GameManager: tutorialUIActor has no Canvas.");
	}
	else
	{
		DBG("GameManager: tutorialUIActor is not found.");
	}

	if (resultUIActorPtr)
	{
		if (!SetUICanvasVisible(*resultUIActorPtr, false))
			DBG("GameManager: resultUIActor has no Canvas.");
	}
	else
	{
		DBG("GameManager: resultUIActor is not found.");
	}

	SetUIImageVisible(*scene, m_clearImageActor, false, "clearImageActor");
	SetUIImageVisible(*scene, m_failedImageActor, false, "failedImageActor");
	SetUIImageVisible(*scene, m_titlePromptImageActor, false, "titlePromptImageActor");
}

void GameManager::PreUpdate() {}

void GameManager::Update() 
{
	auto inputInfo = InputManager::GetInstance().GetInputInfo();

    switch (m_gameState)
    {
    case GameState::Tutorial:
		if (inputInfo.mouse.left.trigger)
		{
			m_player->SetCanMove(true);
			auto tutorialUIActorPtr = m_tutorialUIActor.Resolve(*GetOwner()->GetOwner());
			if (tutorialUIActorPtr)
			{
				if (!SetUICanvasVisible(*tutorialUIActorPtr, false))
					DBG("GameManager: tutorialUIActor has no Canvas.");
			}
			else
			{
				DBG("GameManager: tutorialUIActor is not found.");
			}
			m_gameState = GameState::Playing;
		}

        break;
    case GameState::Playing:

		m_restTime -= TimeManager::GetInstance().GetDeltaTime();

		if (m_restTime <= 0.0f || m_restTarget <= 0)
		{
			m_player->SetCanMove(false);

			SceneBase& scene = *GetOwner()->GetOwner();
			const GameResult result = (m_restTime > 0.0f && m_restTarget <= 0)
				? GameResult::Win : GameResult::Lose;
			const bool won = result == GameResult::Win;

			SetUIImageVisible(scene, m_clearImageActor, won, "clearImageActor");
			SetUIImageVisible(scene, m_failedImageActor, !won, "failedImageActor");
			SetUIImageVisible(scene, m_titlePromptImageActor, true, "titlePromptImageActor");

			auto timerUIActorPtr = m_timerUIActor.Resolve(scene);
			if (timerUIActorPtr)
			{
				if (!SetUICanvasVisible(*timerUIActorPtr, won))
					DBG("GameManager: timerUIActor has no Canvas.");
			}
			else
			{
				DBG("GameManager: timerUIActor is not found.");
			}

			auto resultUIActorPtr = m_resultUIActor.Resolve(scene);
			if (resultUIActorPtr)
			{
				if (!SetUICanvasVisible(*resultUIActorPtr, true))
					DBG("GameManager: resultUIActor has no Canvas.");
			}
			else
			{
				DBG("GameManager: resultUIActor is not found.");
			}

			m_gameState = GameState::Result;
		}

        break;

    case GameState::Result:
        if (inputInfo.mouse.left.trigger)
        {
			auto guid = m_nextScene.GetGuid();
			if (!guid.IsValid())
			{
				DBG("GameManager: nextScene AssetReference is invalid.");
				return;
			}
			ChangeScene(guid);
        }

        break;
    default:
        break;
    }

}

void GameManager::LateUpdate() {}
void GameManager::Destroy() {}
