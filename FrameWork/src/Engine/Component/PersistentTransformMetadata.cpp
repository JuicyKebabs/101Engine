#include "PersistentComponentMetadata.h"
#include "PersistentEnumMetadata.h"
#include "PersistentMetadataHelpers.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include <cmath>

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::Transform(std::string stableTypeName)
{
	TypeMetadataBuilder<::Transform> builder(std::move(stableTypeName));
	PersistentMetadata::AddComponentName(builder);
	builder.Property("position", &::Transform::GetLocalPosition, &::Transform::SetLocalPosition)
		.Validate(ValueValidation::Finite3);
	builder.Property("rotation", &::Transform::GetLocalRotationQuat, &::Transform::SetAuthoredLocalRotation);
	builder.Property("scale", &::Transform::GetLocalScale, &::Transform::SetLocalScale)
		.Validate(ValueValidation::Finite3);
	return PersistentMetadata::Finish(builder);
}


std::unique_ptr<TypeMetadata> PersistentComponentMetadata::RectTransform(std::string stableTypeName)
{
	TypeMetadataBuilder<::RectTransform> builder(std::move(stableTypeName));
	PersistentMetadata::AddComponentName(builder);
	builder.Property("position", &::RectTransform::GetLocalPosition, &::RectTransform::SetLocalPosition)
		.Validate(ValueValidation::Finite3);
	builder.Property("rotation", &::RectTransform::GetLocalRotationQuat, &::RectTransform::SetAuthoredLocalRotation);
	builder.Property("scale", &::RectTransform::GetLocalScale, &::RectTransform::SetLocalScale)
		.Validate(ValueValidation::Finite3);
	builder.Property("anchorMode", &::RectTransform::GetAnchorMode, &::RectTransform::SetAnchorMode)
		.SerializedAs(EnumSerializationFormat::Integer);
	builder.Property("anchoredPosition", &::RectTransform::GetAnchoredPosition, &::RectTransform::SetAnchoredPosition)
		.Validate(ValueValidation::Finite2);
	builder.Property("pivot", &::RectTransform::GetPivot, &::RectTransform::SetPivot).Validate(ValueValidation::UnitCoordinate2);
	builder.Property("size", &::RectTransform::GetSize, &::RectTransform::SetSizeDelta)
		.Validate([](const Vector2& value) { return ValueValidation::Finite2(value) && value.x >= 0.0f && value.y >= 0.0f; });
	return PersistentMetadata::Finish(builder);
}
