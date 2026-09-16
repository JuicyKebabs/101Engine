#include "TitleManager.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/UI/UIImage.h"
#include <algorithm>

REGISTER_GAME_COMPONENT(TitleManager)

std::optional<TypeMetadata> TitleManager::BuildMetadata()
{
    TypeMetadataBuilder<TitleManager> builder("TitleManager");

    /*---- Describe the property registration process ----*/
    // Example: register a private member declared in TitleManager.
    // builder.Property("speed", &TitleManager::m_speed);

	builder.Property("nextScene", &TitleManager::m_nextScene).Optional();
	builder.Property("FadeImageActor", &TitleManager::m_fadeImageActor).Optional();
    return builder.Build();
}

void TitleManager::Start()
{
	UIImage* fadeImage = nullptr;
	if (auto fadeImageActorPtr = m_fadeImageActor.Resolve(*GetOwner()->GetOwner()))
	{
		fadeImage = fadeImageActorPtr->GetComponentByClass<UIImage>();
		if (!fadeImage)
		{
			DBG("TitleManager: FadeImageActor has no UIImage component.");
		}
	}
	else
	{
		DBG("TitleManager: FadeImageActor is not found.");
	}
	m_fadeImage = fadeImage;

	if (m_fadeImage)
	{
		// フェードアウト用のUIImageのアルファ値を0に設定
		Vector4 color = m_fadeImage->GetColor();
		color.w = 0.0f;
		m_fadeImage->SetColor(color);
	}
	else
	{
		DBG("TitleManager: FadeImage is null.");
	}

	m_isFadingOut = false;
}

void TitleManager::PreUpdate() {}

void TitleManager::Update() 
{
	// The Title image prompts the player to left-click to start.
	if (!m_isFadingOut && InputManager::GetInstance().GetInputInfo().mouse.left.trigger)
	{
		m_isFadingOut = true;
	}

	if (m_isFadingOut && m_fadeImage)
	{
		float deltaTime = TimeManager::GetInstance().GetDeltaTime();
		float currentAlpha = m_fadeImage->GetColor().w;
		float newAlpha = std::min(1.0f, currentAlpha + (deltaTime / m_fadeDuration));
		Vector4 color = m_fadeImage->GetColor();
		color.w = newAlpha;
		m_fadeImage->SetColor(color);
		DBG("TitleManager: Fading out, alpha = %f", newAlpha);

		if (newAlpha >= 1.0f)
		{
			auto guid = m_nextScene.GetGuid();
			if (!guid.IsValid())
			{
				DBG("TitleManager: nextScene AssetReference is invalid.");
				return;
			}
			if (ChangeScene(guid)) m_isFadingOut = false;
			else DBG("TitleManager: Failed to reserve the next Scene.");
		}
	}
}

void TitleManager::LateUpdate() {}
void TitleManager::Destroy() {}
