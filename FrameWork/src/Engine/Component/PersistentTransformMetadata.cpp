#include "PersistentComponentMetadata.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/Transform.h"
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

	bool IsFinite(const Vector2& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y);
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

	bool IsNormalizedCoordinate(const Vector2& value)
	{
		if (!IsFinite(value))
		{
			return false;
		}
		const bool xInRange = value.x >= 0.0f && value.x <= 1.0f;
		const bool yInRange = value.y >= 0.0f && value.y <= 1.0f;
		return xInRange && yInRange;
	}
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::Transform()
{
	TypeMetadataBuilder<::Transform> builder("Transform");
	AddComponentName(builder);
	builder.AddAccessorProperty<Vector3>(
		"position", PropertyLogicalType::Vector3, kPolicy,
		[](const ::Transform& component, Vector3& value) { value = component.GetLocalPosition(); return true; },
		[](::Transform& component, const Vector3& value)
		{
			if (!IsFinite(value)) return false;
			component.SetLocalPosition(value);
			return true;
		});
	builder.AddAccessorProperty<Quaternion>(
		"rotation", PropertyLogicalType::Quaternion, kPolicy,
		[](const ::Transform& component, Quaternion& value) { value = component.GetLocalRotationQuat(); return true; },
		[](::Transform& component, const Quaternion& value)
		{
			if (!IsValidRotation(value)) return false;
			component.SetLocalRotationQuat(value.Normalized());
			return true;
		});
	builder.AddAccessorProperty<Vector3>(
		"scale", PropertyLogicalType::Vector3, kPolicy,
		[](const ::Transform& component, Vector3& value) { value = component.GetLocalScale(); return true; },
		[](::Transform& component, const Vector3& value)
		{
			if (!IsFinite(value)) return false;
			component.SetLocalScale(value);
			return true;
		});
	return Finish(builder);
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::RectTransform()
{
	TypeMetadataBuilder<::RectTransform> builder("RectTransform");
	AddComponentName(builder);
	builder.AddAccessorProperty<Vector3>(
		"position", PropertyLogicalType::Vector3, kPolicy,
		[](const ::RectTransform& component, Vector3& value) { value = component.GetLocalPosition(); return true; },
		[](::RectTransform& component, const Vector3& value)
		{
			if (!IsFinite(value)) return false;
			component.SetLocalPosition(Vector3::Zero());
			return true;
		});
	builder.AddAccessorProperty<Quaternion>(
		"rotation", PropertyLogicalType::Quaternion, kPolicy,
		[](const ::RectTransform& component, Quaternion& value) { value = component.GetLocalRotationQuat(); return true; },
		[](::RectTransform& component, const Quaternion& value)
		{
			if (!IsValidRotation(value)) return false;
			component.SetLocalRotationQuat(value.Normalized());
			return true;
		});
	builder.AddAccessorProperty<Vector3>(
		"scale", PropertyLogicalType::Vector3, kPolicy,
		[](const ::RectTransform& component, Vector3& value) { value = component.GetLocalScale(); return true; },
		[](::RectTransform& component, const Vector3& value)
		{
			if (!IsFinite(value)) return false;
			component.SetLocalScale(value);
			return true;
		});
	auto anchor = EnumMetadataBuilder<AnchorMode>()
		.Add("TopLeft", AnchorMode::TopLeft).Add("TopCenter", AnchorMode::TopCenter)
		.Add("TopRight", AnchorMode::TopRight).Add("MiddleLeft", AnchorMode::MiddleLeft)
		.Add("MiddleCenter", AnchorMode::MiddleCenter).Add("MiddleRight", AnchorMode::MiddleRight)
		.Add("BottomLeft", AnchorMode::BottomLeft).Add("BottomCenter", AnchorMode::BottomCenter)
		.Add("BottomRight", AnchorMode::BottomRight).Build();
	if (!anchor) return nullptr;
	builder.AddAccessorProperty<AnchorMode>(
		"anchorMode", PropertyLogicalType::Enum, kPolicy,
		[](const ::RectTransform& component, AnchorMode& value) { value = component.GetAnchorMode(); return true; },
		[](::RectTransform& component, AnchorMode value) { component.SetAnchorMode(value); return true; },
		*anchor, EnumSerializationFormat::Integer);
	builder.AddAccessorProperty<Vector2>(
		"anchoredPosition", PropertyLogicalType::Vector2, kPolicy,
		[](const ::RectTransform& component, Vector2& value) { value = component.GetAnchoredPosition(); return true; },
		[](::RectTransform& component, const Vector2& value)
		{
			if (!IsFinite(value)) return false;
			component.SetAnchoredPosition(value);
			return true;
		});
	builder.AddAccessorProperty<Vector2>(
		"pivot", PropertyLogicalType::Vector2, kPolicy,
		[](const ::RectTransform& component, Vector2& value) { value = component.GetPivot(); return true; },
		[](::RectTransform& component, const Vector2& value)
		{
			if (!IsNormalizedCoordinate(value))
			{
				return false;
			}
			component.SetPivot(value);
			return true;
		});
	builder.AddAccessorProperty<Vector2>(
		"size", PropertyLogicalType::Vector2, kPolicy,
		[](const ::RectTransform& component, Vector2& value) { value = component.GetSize(); return true; },
		[](::RectTransform& component, const Vector2& value)
		{
			if (!IsFinite(value) || value.x < 0.0f || value.y < 0.0f) return false;
			component.SetSizeDelta(value);
			return true;
		});
	return Finish(builder);
}


