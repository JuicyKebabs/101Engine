#include "PersistentComponentMetadata.h"
#include "Engine/Component/Collider.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include <cmath>

namespace
{
	constexpr PropertyPolicy kPolicy = PropertyPolicy::Serializable | PropertyPolicy::Inspectable;

	template<class ComponentType>
	void AddComponentName(TypeMetadataBuilder<ComponentType>& builder)
	{
		builder.template AddAccessorProperty<std::string>(
			"name", PropertyLogicalType::String, kPolicy,
			[](const ComponentType& component, std::string& value)
			{
				value = component.GetName();
				return true;
			},
			[](ComponentType& component, const std::string& value)
			{
				component.SetName(value);
				return true;
			});
	}

	bool IsFinite(const Vector3& value)
	{
		return std::isfinite(value.x)
			&& std::isfinite(value.y)
			&& std::isfinite(value.z);
	}

	bool IsValidRotation(const Quaternion& value)
	{
		const float lengthSquared = value.LengthSq();
		return std::isfinite(lengthSquared) && lengthSquared > 0.000001f;
	}

	template<class ComponentType>
	std::unique_ptr<TypeMetadata> Finish(TypeMetadataBuilder<ComponentType>& builder)
	{
		auto metadata = builder.Build();
		if (!metadata)
		{
			return nullptr;
		}
		return std::make_unique<TypeMetadata>(std::move(*metadata));
	}
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::Collider()
{
	TypeMetadataBuilder<::Collider> builder("Collider");
	AddComponentName(builder);
	auto colliderType = EnumMetadataBuilder<ColliderType>()
		.Add("BOX", ColliderType::BOX).Add("SPHERE", ColliderType::SPHERE)
		.Add("CAPSULE", ColliderType::CAPSULE).Add("None", ColliderType::None).Build();
	auto collisionLayer = EnumMetadataBuilder<CollisionLayer>()
		.Add("Default", CollisionLayer::Default).Add("PLAYER", CollisionLayer::PLAYER)
		.Add("ENEMY", CollisionLayer::ENEMY).Add("WALL", CollisionLayer::WALL)
		.Add("PLAYER_BULLET", CollisionLayer::PLAYER_BULLET).Add("PLAYER_RAY", CollisionLayer::PLAYER_RAY)
		.Add("ENEMY_BULLET", CollisionLayer::ENEMY_BULLET).Build();
	if (!colliderType || !collisionLayer) return nullptr;
	builder.AddAccessorProperty<Vector3>(
		"center", PropertyLogicalType::Vector3, kPolicy,
		[](const ::Collider& component, Vector3& value) { value = component.m_localTransform.position; return true; },
		[](::Collider& component, const Vector3& value)
		{
			if (!IsFinite(value)) return false;
			component.SetLocalCenter(value); return true;
		});
	builder.AddAccessorProperty<Quaternion>(
		"rotation", PropertyLogicalType::Quaternion, kPolicy,
		[](const ::Collider& component, Quaternion& value) { value = component.m_localTransform.rotation; return true; },
		[](::Collider& component, const Quaternion& value)
		{
			if (!IsValidRotation(value)) return false;
			component.SetLocalRotation(value.Normalized()); return true;
		});
	builder.AddAccessorProperty<Vector3>(
		"scale", PropertyLogicalType::Vector3, kPolicy,
		[](const ::Collider& component, Vector3& value) { value = component.m_localTransform.scale; return true; },
		[](::Collider& component, const Vector3& value)
		{
			if (!IsFinite(value))
			{
				return false;
			}
			const bool hasPositiveScale = value.x > 0.0f
				&& value.y > 0.0f
				&& value.z > 0.0f;
			if (!hasPositiveScale)
			{
				return false;
			}
			component.SetLocalScale(value); return true;
		});
	builder.AddAccessorProperty<ColliderType>(
		"type", PropertyLogicalType::Enum, kPolicy,
		[](const ::Collider& component, ColliderType& value) { value = component.m_type; return true; },
		[](::Collider& component, ColliderType value)
		{
			component.SetType(value);
			component.m_collisionInfos.clear();
			component.m_isDetected = false;
			component.m_deleteFlag = false;
			component.m_isActive = true;
			component.m_transformGeneration = static_cast<std::uint64_t>(-1);
			component.m_isDirty = true;
			return true;
		}, *colliderType, EnumSerializationFormat::Integer);
	builder.AddAccessorProperty<CollisionLayer>(
		"layer", PropertyLogicalType::Enum, kPolicy,
		[](const ::Collider& component, CollisionLayer& value) { value = component.m_layer; return true; },
		[](::Collider& component, CollisionLayer value) { component.SetLayer(value); return true; },
		*collisionLayer, EnumSerializationFormat::Integer);
	builder.AddAccessorProperty<bool>(
		"isTrigger", PropertyLogicalType::Bool, kPolicy,
		[](const ::Collider& component, bool& value) { value = component.m_isTrigger; return true; },
		[](::Collider& component, bool value) { component.SetTrigger(value); return true; });
	return Finish(builder);
}


