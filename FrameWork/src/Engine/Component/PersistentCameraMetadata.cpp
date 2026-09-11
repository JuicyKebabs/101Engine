#include "PersistentComponentMetadata.h"
#include "PersistentEnumMetadata.h"
#include "PersistentMetadataHelpers.h"
#include "Engine/Component/Camera.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include <cmath>

namespace
{
	bool Positive(float v) { return std::isfinite(v) && v > 0; }
	bool Nonnegative(float v) { return std::isfinite(v) && v >= 0; }
	InspectorMetadata FovInspector()
	{
		NumericEditorMetadata numeric;
		numeric.dragSpeed = 0.5f;
		numeric.minimum = 1.0;
		numeric.maximum = 179.0;
		numeric.storageUnit = NumericUnit::Radians;
		numeric.displayUnit = NumericUnit::Degrees;
		InspectorMetadata inspector;
		inspector.label = "Field of View";
		inspector.numeric = numeric;
		return inspector;
	}
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::Camera(std::string stableTypeName)
{
	using T = ::Camera;
	TypeMetadataBuilder<T> builder(std::move(stableTypeName));
	builder.SetValidator([](const T& component) -> std::optional<ReflectionError>
	{
		const auto lens = component.GetCameraLens();
		if (lens.nearZ < lens.farZ) return std::nullopt;
		return ReflectionError{ReflectionErrorCode::TypeInvariantViolation,
			PropertyPath::FromString("/lens/farZ"), "Camera farZ must be greater than nearZ."};
	});
	PersistentMetadata::AddComponentName(builder);
	builder.Property("followMode", &T::GetFollowMode, &T::SetFollowMode).SerializedAs(EnumSerializationFormat::Integer);
	builder.Property("rotationMode", &T::GetRotationMode, &T::SetRotationMode).SerializedAs(EnumSerializationFormat::Integer);
	builder.Property("targetActorId", &T::GetTargetActorReference, &T::SetTargetActorReference);
	builder.Property("followActorId", &T::GetFollowActorReference, &T::SetFollowActorReference);
	builder.Object("rig", [](auto& rig)
	{
		rig.Property("offsetPosition", &T::GetCameraRig, &T::SetCameraRig, &CameraRig::offsetPosition).Validate(ValueValidation::Finite3);
		rig.Property("offsetRotation", [](const T& c) { return c.GetCameraRig().offsetRotation; }, &T::SetAuthoredRigRotation);
	});
	builder.Object("pose", [](auto& pose)
	{
		pose.Property("position", &T::GetCameraPose, &T::SetCameraPose, &CameraPose::position).Validate(ValueValidation::Finite3);
		pose.Property("rotation", [](const T& c) { return c.GetCameraPose().rotation; }, &T::SetAuthoredPoseRotation);
	});
	builder.Object("lens", [](auto& lens)
	{
		lens.Property("fov", &T::GetCameraLens, &T::SetCameraLens, &CameraLens::fov).Validate(Positive).Inspector(FovInspector());
		lens.Property("width", &T::GetCameraLens, &T::SetCameraLens, &CameraLens::width).Validate(Nonnegative);
		lens.Property("height", &T::GetCameraLens, &T::SetCameraLens, &CameraLens::height).Validate(Nonnegative);
		lens.Property("nearZ", &T::GetCameraLens, &T::SetCameraLens, &CameraLens::nearZ).Validate(Positive);
		lens.Property("farZ", &T::GetCameraLens, &T::SetCameraLens, &CameraLens::farZ).Validate(Positive);
		lens.Property("projectionType", &T::GetCameraLens, &T::SetCameraLens, &CameraLens::projectionType).SerializedAs(EnumSerializationFormat::Integer);
	});
	return PersistentMetadata::Finish(builder);
}
