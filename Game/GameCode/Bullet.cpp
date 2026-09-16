#include "Bullet.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Time/Time.h"
#include "FixedTarget.h"

REGISTER_GAME_COMPONENT(Bullet)

std::optional<TypeMetadata> Bullet::BuildMetadata()
{
    TypeMetadataBuilder<Bullet> builder("Bullet");

    /*---- Describe the property registration process ----*/
    // Example: register a private member declared in Bullet.
    // builder.Property("speed", &Bullet::m_speed);

	builder.Property("damage", &Bullet::m_damage).Optional();
	builder.Property("lifeTime", &Bullet::m_lifeTime).Optional();

    return builder.Build();
}

void Bullet::Start() 
{
	m_collider = GetOwner()->GetComponentByClass<Collider>();
}

void Bullet::PreUpdate() {}

void Bullet::Update() 
{
	auto owner = GetOwner();
	Transform* transform = owner ? owner->GetComponentByClass<Transform>() : nullptr;
	auto deltaTime = TimeManager::GetInstance().GetDeltaTime();

	if (transform)
	{
		
		transform->SetLocalPosition(transform->GetLocalPosition() + m_direction * m_speed * deltaTime);
	}

	m_lifeTime -= deltaTime;

	if (m_lifeTime <= 0.0f)
	{
		auto owner = GetOwner();
		if (owner)
		{
			owner->Destroy();
		}
		DBG("Bullet destroyed due to lifetime expiration.");
	}
}

void Bullet::LateUpdate() 
{
	auto collisionInfo = m_collider ? m_collider->GetCollisionInfos() : std::vector<CollisionInfo>();

	for (const auto& info : collisionInfo)
	{
		if (info.opponent)
		{
			auto opponentActor = info.opponent->GetOwner();
			if (opponentActor)
			{
				DBG("Bullet collided with: %s", opponentActor->GetName().c_str());

				auto tag = opponentActor->GetTag();
				auto tagRegistry = TagRegistry::Get();
				auto tagName = tagRegistry.GetName(tag);

				if (tagName == "Enemy")
				{
					FixedTarget* target = opponentActor->GetComponentByClass<FixedTarget>();
					if (target)
					{
						// ターゲットの破壊時処理を呼び出す
						target->Break();
					}

					opponentActor->Destroy();
				}

				auto owner = GetOwner();
				if (owner)
				{
					owner->Destroy();
				}
				DBG("Bullet destroyed due to collision.");
			}
		}
	}
}


void Bullet::Destroy() {}

void Bullet::Launch(const Vector3& position, const Vector3& direction, float speed)
{
	m_direction = direction.Normalized();
	m_speed = speed;

	auto owner = GetOwner();
	Transform* transform = owner ? owner->GetComponentByClass<Transform>() : nullptr;
	if (transform)
	{
		transform->SetLocalPosition(position);
	}
}
