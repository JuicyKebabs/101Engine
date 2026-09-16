#include "Explosion.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Core/Time/Time.h"

REGISTER_GAME_COMPONENT(Explosion)

std::optional<TypeMetadata> Explosion::BuildMetadata()
{
    TypeMetadataBuilder<Explosion> builder("Explosion");

    /*---- Describe the property registration process ----*/
    // Example: register a private member declared in Explosion.
    // builder.Property("speed", &Explosion::m_speed);

	builder.Property("duration", &Explosion::m_duration).Optional();
	builder.Property("elapsedTime", &Explosion::m_elapsedTime).Optional();
	builder.Property("spriteIndex", &Explosion::m_spriteIndex).Optional();
	builder.Property("totalSprites", &Explosion::m_totalSprites).Optional();

    return builder.Build();
}

void Explosion::Start() 
{
	auto owner = GetOwner();
	SpriteRenderer* spriteRenderer = owner ? owner->GetComponentByClass<SpriteRenderer>() : nullptr;
	m_spriteRenderer = spriteRenderer;

	m_spriteIndex = 0;
	m_elapsedTime = 0.0f;
}
void Explosion::PreUpdate() {}
void Explosion::Update()
{
	if (!m_spriteRenderer)
	{
		DBG("Explosion: SpriteRenderer is not found.");
		return;
	}

	const float deltaTime = TimeManager::GetInstance().GetDeltaTime();

	m_elapsedTime += deltaTime;
	m_spriteIndex = static_cast<int>((m_elapsedTime / m_duration) * m_totalSprites);

	if (m_spriteIndex >= m_totalSprites)
	{
		auto owner = GetOwner();
		if (owner)
		{
			owner->Destroy();
		}
		else
		{
			DBG("Explosion: Owner actor is not found.");
		}
	}
	else
	{
		const float uvScaleX = 1.0f / m_totalSprites;
		const Vector2 uvScale{ uvScaleX, 1.0f };
		const Vector2 uvOffset{ uvScaleX * m_spriteIndex, 0.0f };
		m_spriteRenderer->SetUVScale(uvScale);
		m_spriteRenderer->SetUVOffset(uvOffset);
	}
}

void Explosion::LateUpdate() {}
void Explosion::Destroy() {}
