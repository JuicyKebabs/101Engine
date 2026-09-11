#include "PersistentComponentMetadata.h"
#include "Engine/Component/Camera.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Resource/AssetReference.h"
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

	InspectorMetadata MakeFovInspectorMetadata()
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

	std::optional<ReflectionError> ValidateCamera(const ::Camera& component)
	{
		const CameraLens lens = component.GetCameraLens();
		const float nearZ = lens.nearZ;
		const float farZ = lens.farZ;
		if (nearZ < farZ)
		{
			return std::nullopt;
		}

		return ReflectionError{
			ReflectionErrorCode::TypeInvariantViolation,
			PropertyPath::FromString("/lens/farZ"),
			"Camera farZ must be greater than nearZ." };
	}
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::Camera()
{
	TypeMetadataBuilder<::Camera> builder("Camera");
	builder.SetValidator(ValidateCamera);
	AddComponentName(builder);

	auto followMode = EnumMetadataBuilder<CAMERA_FOLLOW_MODE>()
		.Add("FOLLOW_MODE_FIXED", CAMERA_FOLLOW_MODE::FOLLOW_MODE_FIXED)
		.Add("FOLLOW_MODE_OWNER", CAMERA_FOLLOW_MODE::FOLLOW_MODE_OWNER)
		.Add("FOLLOW_MODE_TARGET", CAMERA_FOLLOW_MODE::FOLLOW_MODE_TARGET).Build();
	auto rotationMode = EnumMetadataBuilder<CAMERA_ROTATION_MODE>()
		.Add("ROTATION_MODE_FIXED", CAMERA_ROTATION_MODE::ROTATION_MODE_FIXED)
		.Add("ROTATION_MODE_MATCH_OWNER", CAMERA_ROTATION_MODE::ROTATION_MODE_MATCH_OWNER)
		.Add("ROTATION_MODE_LOOK_AT_TARGET", CAMERA_ROTATION_MODE::ROTATION_MODE_LOOK_AT_TARGET).Build();
	auto projectionType = EnumMetadataBuilder<PROJECTION_TYPE>()
		.Add("PROJECTION_TYPE_PERSPECTIVE", PROJECTION_TYPE::PROJECTION_TYPE_PERSPECTIVE)
		.Add("PROJECTION_TYPE_ORTHOGRAPHIC", PROJECTION_TYPE::PROJECTION_TYPE_ORTHOGRAPHIC).Build();
	if (!followMode || !rotationMode || !projectionType)
	{
		return nullptr;
	}

	builder.AddAccessorProperty<CAMERA_FOLLOW_MODE>(
		"followMode", PropertyLogicalType::Enum, kPolicy,
		[](const ::Camera& component, CAMERA_FOLLOW_MODE& value)
		{
			value = component.m_followMode;
			return true;
		},
		[](::Camera& component, CAMERA_FOLLOW_MODE value)
		{
			component.SetFollowMode(value);
			return true;
		},
		*followMode, EnumSerializationFormat::Integer);
	builder.AddAccessorProperty<CAMERA_ROTATION_MODE>(
		"rotationMode", PropertyLogicalType::Enum, kPolicy,
		[](const ::Camera& component, CAMERA_ROTATION_MODE& value)
		{
			value = component.m_rotationMode;
			return true;
		},
		[](::Camera& component, CAMERA_ROTATION_MODE value)
		{
			component.SetRotationMode(value);
			return true;
		},
		*rotationMode, EnumSerializationFormat::Integer);
	builder.AddAccessorProperty<ActorReference>(
		"targetActorId", PropertyLogicalType::ActorReference, kPolicy,
		[](const ::Camera& component, ActorReference& value)
		{
			value = component.m_targetActor;
			return true;
		},
		[](::Camera& component, const ActorReference& value)
		{
			component.m_targetActor = value;
			component.m_isCameraInfoDirty = true;
			return true;
		});
	builder.AddAccessorProperty<ActorReference>(
		"followActorId", PropertyLogicalType::ActorReference, kPolicy,
		[](const ::Camera& component, ActorReference& value)
		{
			value = component.m_followActor;
			return true;
		},
		[](::Camera& component, const ActorReference& value)
		{
			component.m_followActor = value;
			component.m_isCameraInfoDirty = true;
			component.m_followingTransformGeneration = static_cast<std::uint64_t>(-1);
			component.m_rotatingTransformGeneration = static_cast<std::uint64_t>(-1);
			return true;
		});

	builder.Object("rig", [](auto& rig)
	{
		rig.template AddAccessor<Vector3>(
			"offsetPosition", PropertyLogicalType::Vector3, kPolicy,
			[](const ::Camera& component, Vector3& value)
			{
				value = component.m_cameraRig.offsetPosition;
				return true;
			},
			[](::Camera& component, const Vector3& value)
			{
				if (!IsFinite(value))
				{
					return false;
				}
				component.m_cameraRig.offsetPosition = value;
				component.m_isCameraInfoDirty = true;
				return true;
			});
		rig.template AddAccessor<Quaternion>(
			"offsetRotation", PropertyLogicalType::Quaternion, kPolicy,
			[](const ::Camera& component, Quaternion& value)
			{
				value = component.m_cameraRig.offsetRotation;
				return true;
			},
			[](::Camera& component, const Quaternion& value)
			{
				if (!IsValidRotation(value))
				{
					return false;
				}
				component.m_cameraRig.offsetRotation = value.Normalized();
				component.m_isCameraInfoDirty = true;
				return true;
			});
	});

	builder.Object("pose", [](auto& pose)
	{
		pose.template AddAccessor<Vector3>(
			"position", PropertyLogicalType::Vector3, kPolicy,
			[](const ::Camera& component, Vector3& value)
			{
				value = component.m_cameraPose.position;
				return true;
			},
			[](::Camera& component, const Vector3& value)
			{
				if (!IsFinite(value))
				{
					return false;
				}
				component.m_cameraPose.position = value;
				component.m_isCameraInfoDirty = true;
				return true;
			});
		pose.template AddAccessor<Quaternion>(
			"rotation", PropertyLogicalType::Quaternion, kPolicy,
			[](const ::Camera& component, Quaternion& value)
			{
				value = component.m_cameraPose.rotation;
				return true;
			},
			[](::Camera& component, const Quaternion& value)
			{
				if (!IsValidRotation(value))
				{
					return false;
				}
				component.m_cameraPose.rotation = value.Normalized();
				component.m_isCameraInfoDirty = true;
				return true;
			});
	});

	builder.Object("lens", [&projectionType](auto& lens)
	{
		lens.template AddAccessor<float>(
			"fov",
			[](const ::Camera& component, float& value)
			{
				value = component.m_cameraLens.fov;
				return true;
			},
			[](::Camera& component, float value)
			{
				if (!std::isfinite(value) || value <= 0.0f)
				{
					return false;
				}
				component.m_cameraLens.fov = value;
				component.m_isCameraInfoDirty = true;
				return true;
			},
			kPolicy, PropertyRequirement::Required, MakeFovInspectorMetadata());
		lens.template AddAccessor<float>(
			"width", PropertyLogicalType::Float, kPolicy,
			[](const ::Camera& component, float& value)
			{
				value = component.m_cameraLens.width;
				return true;
			},
			[](::Camera& component, float value)
			{
				if (!std::isfinite(value) || value < 0.0f)
				{
					return false;
				}
				component.m_cameraLens.width = value;
				component.m_isCameraInfoDirty = true;
				return true;
			});
		lens.template AddAccessor<float>(
			"height", PropertyLogicalType::Float, kPolicy,
			[](const ::Camera& component, float& value)
			{
				value = component.m_cameraLens.height;
				return true;
			},
			[](::Camera& component, float value)
			{
				if (!std::isfinite(value) || value < 0.0f)
				{
					return false;
				}
				component.m_cameraLens.height = value;
				component.m_isCameraInfoDirty = true;
				return true;
			});
		lens.template AddAccessor<float>(
			"nearZ",
			[](const ::Camera& component, float& value)
			{
				value = component.m_cameraLens.nearZ;
				return true;
			},
			[](::Camera& component, float value)
			{
				if (!std::isfinite(value) || value <= 0.0f)
				{
					return false;
				}
				component.m_cameraLens.nearZ = value;
				component.m_isCameraInfoDirty = true;
				return true;
			}, kPolicy);
		lens.template AddAccessor<float>(
			"farZ",
			[](const ::Camera& component, float& value)
			{
				value = component.m_cameraLens.farZ;
				return true;
			},
			[](::Camera& component, float value)
			{
				if (!std::isfinite(value) || value <= 0.0f)
				{
					return false;
				}
				component.m_cameraLens.farZ = value;
				component.m_isCameraInfoDirty = true;
				return true;
			}, kPolicy);
		lens.template AddAccessor<PROJECTION_TYPE>(
			"projectionType", PropertyLogicalType::Enum, kPolicy,
			[](const ::Camera& component, PROJECTION_TYPE& value)
			{
				value = component.m_cameraLens.projectionType;
				return true;
			},
			[](::Camera& component, PROJECTION_TYPE value)
			{
				component.m_cameraLens.projectionType = value;
				component.m_isCameraInfoDirty = true;
				return true;
			},
			*projectionType, EnumSerializationFormat::Integer);
	});

	auto metadata = builder.Build();
	if (!metadata)
	{
		return nullptr;
	}
	return std::make_unique<TypeMetadata>(std::move(*metadata));
}
