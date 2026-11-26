#include "AnimationTest.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Input/InputManager.h"

REGISTER_GAME_COMPONENT(AnimationTest)

std::optional<TypeMetadata> AnimationTest::BuildMetadata()
{
    TypeMetadataBuilder<AnimationTest> builder("AnimationTest");

    /*---- Describe the property registration process ----*/
    // Example: register a private member declared in AnimationTest.
    // builder.Property("speed", &AnimationTest::m_speed);

    return builder.Build();
}

void AnimationTest::Start()
{
	auto owner = GetOwner();

    if (owner)
    {
        m_animationPlayer = owner->GetComponentByClass<AnimationPlayer>();
    }
}
void AnimationTest::PreUpdate() {}

void AnimationTest::Update()
{
	if (m_animationPlayer)
	{
		auto inputInfo = InputManager::GetInstance().GetInputInfo().key;

		if (inputInfo.space.trigger)
		{
			std::string currentClipName = m_animationPlayer->GetCurrentClipName();

			if (currentClipName == "Dance1")
			{
				m_animationPlayer->SwitchClip("Dance2", 0.5f);
			}
			else if (currentClipName == "Dance2")
			{
				m_animationPlayer->SwitchClip("Dance1", 0.5f);
			}
		}

		if (inputInfo.enter.trigger)
		{
			m_animationPlayer->RestartCurrentClip();
		}

		if (inputInfo.left.trigger)
		{
			m_animationPlayer->SetIsPlaying(!m_animationPlayer->IsPlaying());
		}
	}
}

void AnimationTest::LateUpdate() {}

void AnimationTest::Destroy() {}
