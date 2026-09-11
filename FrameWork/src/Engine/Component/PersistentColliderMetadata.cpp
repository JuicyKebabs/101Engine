#include "PersistentComponentMetadata.h"
#include "PersistentEnumMetadata.h"
#include "PersistentMetadataHelpers.h"
#include "Engine/Component/Collider.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include <cmath>

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::Collider(std::string stableTypeName)
{
	using T = ::Collider;
	TypeMetadataBuilder<T> builder(std::move(stableTypeName));
	PersistentMetadata::AddComponentName(builder);
	builder.Property("center", &T::GetLocalCenter, &T::SetLocalCenter).Validate(ValueValidation::Finite3);
	builder.Property("rotation", &T::GetLocalRotation, &T::SetAuthoredLocalRotation);
	builder.Property("scale", &T::GetLocalScale, &T::SetLocalScale)
		.Validate([](const Vector3& v) { return ValueValidation::Finite3(v) && v.x > 0 && v.y > 0 && v.z > 0; });
	builder.Property("type", &T::GetType, &T::SetAuthoredType).SerializedAs(EnumSerializationFormat::Integer);
	builder.Property("layer", &T::GetLayer, &T::SetLayer).SerializedAs(EnumSerializationFormat::Integer);
	builder.Property("isTrigger", &T::IsTrigger, &T::SetTrigger);
	return PersistentMetadata::Finish(builder);
}
