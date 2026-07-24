#include <cmath>
#include <string>
#include "Camera.h"
#include "Transform.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Core/Serialization/JsonMath.h"
#include "nlohmann/json.hpp"

void Camera::OnStartOverride()
{
}

void Camera::PreUpdateOverride(float deltaTime)
{
}

void Camera::UpdateOverride(float deltaTime)
{
}

void Camera::LateUpdateOverride(float deltaTime)
{
}

void Camera::OnDestroyOverride()
{
}

void Camera::Flush(float deltaTime)
{
	UpdateCameraPose(deltaTime);
}

const CameraInfo& Camera::GetCameraInfo()
{
	if(m_isCameraInfoDirty)
	{
		m_cameraInfo = RebuildCameraInfo();
		m_isCameraInfoDirty = false; // Mark as clean after rebuilding
	}

	return m_cameraInfo;
}

void Camera::SetAsMainCamera()
{
	auto owner = GetOwner();
	if (owner) {
		auto scene = owner->GetOwner();
		if (scene) {
			auto cameraSystem = scene->GetCameraSystem();
			if (cameraSystem) {
				cameraSystem->SetMainCamera(this);
			}
			else {
				DBG("Camera::SetAsMainCamera() - No CameraSystem found in the scene.\n");
			}
		}
		else {
			DBG("Camera::SetAsMainCamera() - Owning actor does not belong to a scene.\n");
		}
	}
	else{
		DBG("Camera::SetAsMainCamera() - Camera component does not have an owning actor.\n");
	}
}

CameraInfo Camera::RebuildCameraInfo()
{
	CameraInfo info;

	Matrix4x4 world = Matrix4x4::Identity;
	world *= m_cameraPose.rotation.ToRotationMatrix();
	world *= Matrix4x4::CreateTranslation(m_cameraPose.position); // Apply inverse translation for view matrix
	info.viewMatrix = world.Inverse();

	// Build projection matrix
	if (m_cameraLens.projectionType == PROJECTION_TYPE::PROJECTION_TYPE_PERSPECTIVE)
	{
		const float aspectRatio = m_cameraLens.width / m_cameraLens.height;
		info.projMatrix = Matrix4x4::CreatePerspectiveFov(m_cameraLens.fov, aspectRatio, m_cameraLens.nearZ, m_cameraLens.farZ);
	}
	else // Orthographic projection
	{
		float orthoWidth = m_cameraLens.width;
		float orthoHeight = m_cameraLens.height;
		info.projMatrix = Matrix4x4::CreateOrthographic(orthoWidth, orthoHeight, m_cameraLens.nearZ, m_cameraLens.farZ);
	}

	info.position = m_cameraPose.position;
	info.forward = m_cameraPose.rotation.RotateVector3(Vector3::Forward());
	info.up = m_cameraPose.rotation.RotateVector3(Vector3::Up());

	m_cameraInfo = info; // Cache the camera information

	return m_cameraInfo;
}

void Camera::UpdateCameraPose(float deltaTime)
{
	Actor* followingTarget = nullptr;	// Actor that the camera is following (for position updates)
	Actor* rotatingTarget = nullptr;	// Actor that the camera is rotating with (for rotation updates)
	bool followingDirty = false;		// Flag to indicate if position needs to be updated
	bool rotatingDirty = false;			// Flag to indicate if rotation needs to be updated

	// Set following target based on follow mode
	switch (m_followMode)
	{
	case CAMERA_FOLLOW_MODE::FOLLOW_MODE_FIXED:
		break;
	case CAMERA_FOLLOW_MODE::FOLLOW_MODE_OWNER:
		followingTarget = GetOwner();
		break;
	case CAMERA_FOLLOW_MODE::FOLLOW_MODE_TARGET:
		followingTarget = m_pFollowActor;
		break;
	default:
		break;
	}

	// Check if the following target's transform has changed since the last update
	if (followingTarget) {
		auto followTransform = followingTarget->GetComponentByClass<Transform>();
		if (followTransform) {
			if (m_followingTransformGeneration != followTransform->GetWorldGeneration()) {
				m_followingTransformGeneration = followTransform->GetWorldGeneration();
				m_isCameraInfoDirty = true;	// Mark camera info as dirty to trigger rebuild
				followingDirty = true;
			}
		}
	}

	// Set rotating target based on rotation mode
	switch (m_rotationMode)
	{
	case CAMERA_ROTATION_MODE::ROTATION_MODE_FIXED:
		break;
	case CAMERA_ROTATION_MODE::ROTATION_MODE_MATCH_OWNER:
		rotatingTarget = GetOwner();
		break;
	case CAMERA_ROTATION_MODE::ROTATION_MODE_LOOK_AT_TARGET:
		rotatingTarget = m_pTargetActor;
		rotatingDirty |= followingDirty;	// If the following target is dirty, set rotating dirty as well
		break;
	default:
		break;
	}

	// Check if the rotating target's transform has changed since the last update
	if (rotatingTarget) {
		auto rotatingTransform = rotatingTarget->GetComponentByClass<Transform>();
		if (rotatingTransform) {
			if (m_rotatingTransformGeneration != rotatingTransform->GetWorldGeneration()) {
				m_rotatingTransformGeneration = rotatingTransform->GetWorldGeneration();
				m_isCameraInfoDirty = true;	// Mark camera info as dirty to trigger rebuild
				rotatingDirty = true;
			}
		}
	}	

	// Update position and rotation if needed
	if (followingDirty) UpdatePosition(deltaTime);
	if (rotatingDirty)  UpdateRotation(deltaTime);
}

void Camera::UpdatePosition(float deltaTime)
{
	Actor* followTarget = nullptr;

	switch (m_followMode)
	{
	case CAMERA_FOLLOW_MODE::FOLLOW_MODE_FIXED:
		return; // Do not update position, keep it fixed
		break;
	case CAMERA_FOLLOW_MODE::FOLLOW_MODE_OWNER:
		followTarget = GetOwner();
		break;
	case CAMERA_FOLLOW_MODE::FOLLOW_MODE_TARGET:
		followTarget = m_pFollowActor;
		break;
	default:
		break;
	}
	if (!followTarget) return; // No valid follow target, do not update position

	auto targetTransform = followTarget->GetComponentByClass<Transform>();
	if (!targetTransform) return; // Follow target does not have a Transform component, do not update position

	Vector3 newPosition = targetTransform->TransformPoint(m_cameraRig.offsetPosition); // Apply offset position from the camera rig
	m_cameraPose.position = newPosition;
}

void Camera::UpdateRotation(float deltaTime)
{
	Quaternion targetRotation = Quaternion::Identity();

	switch (m_rotationMode)
	{
	case CAMERA_ROTATION_MODE::ROTATION_MODE_FIXED:
		targetRotation = m_cameraPose.rotation; // Keep current rotation
		break;

	case CAMERA_ROTATION_MODE::ROTATION_MODE_MATCH_OWNER:
		if (GetOwner()) {
			auto ownerTransform = GetOwner()->GetComponentByClass<Transform>();
			if (ownerTransform) {
				targetRotation = ownerTransform->GetWorldRotationQuat();
				// Apply offset rotation from the camera rig
				targetRotation *= m_cameraRig.offsetRotation;
			}
		}
		break;

	case CAMERA_ROTATION_MODE::ROTATION_MODE_LOOK_AT_TARGET:
		if (m_pTargetActor) {
			auto targetTransform = m_pTargetActor->GetComponentByClass<Transform>();
			if (targetTransform) {
				Vector3 targetPosition = targetTransform->GetWorldPosition();
				Vector3 cameraPosition = m_cameraPose.position;
				Vector3 direction = {
					targetPosition.x - cameraPosition.x,
					targetPosition.y - cameraPosition.y,
					targetPosition.z - cameraPosition.z
				};
				// Calculate yaw and pitch to look at the target
				float yaw = atan2f(direction.x, direction.z);
				float distanceXZ = sqrtf(direction.x * direction.x + direction.z * direction.z);
				float pitch = atan2f(direction.y, distanceXZ);
				targetRotation = Quaternion::CreateFromEulerRad(Vector3(pitch, yaw, 0.0f));
				// Apply offset rotation from the camera rig
				targetRotation *= m_cameraRig.offsetRotation;
			}
		}
		break;
	default:
		break;
	}

	m_cameraPose.rotation = targetRotation;
}

bool Camera::Serialize(nlohmann::json& outJson) const
{
	if (!Component::Serialize(outJson)) return false;

	// Name of the camera component
	outJson["name"] = GetName();

	// Mode parameters
	outJson["followMode"] = static_cast<int>(m_followMode);
	outJson["rotationMode"] = static_cast<int>(m_rotationMode);

	// Rig parameters
	outJson["rig"] = {
		{"offsetPosition", JsonMath::ToJson(m_cameraRig.offsetPosition)},
		{"offsetRotation", JsonMath::ToJson(m_cameraRig.offsetRotation)}
	};

	// Pose parameters
	outJson["pose"] = {
		{"position", JsonMath::ToJson(m_cameraPose.position)},
		{"rotation", JsonMath::ToJson(m_cameraPose.rotation)}
	};

	// Lens parameters
	outJson["lens"] = {
		{"fov", m_cameraLens.fov},
		{"width", m_cameraLens.width},
		{"height", m_cameraLens.height},
		{"nearZ", m_cameraLens.nearZ},
		{"farZ", m_cameraLens.farZ},
		{"projectionType", static_cast<int>(m_cameraLens.projectionType)}
	};

	// Reference to target(by GUID)
	if (m_pTargetActor)
	{
		outJson["targetActorId"] = m_pTargetActor->GetGuid().ToString();
	}
	else if (m_pendingTargetActorGuid.has_value())
	{
		outJson["targetActorId"] = m_pendingTargetActorGuid->ToString();
	}
	else
	{
		outJson["targetActorId"] = nullptr;
	}

	// Reference to follow target(by GUID)
	if (m_pFollowActor)
	{
		outJson["followActorId"] = m_pFollowActor->GetGuid().ToString();
	}
	else if (m_pendingFollowActorGuid.has_value())
	{
		outJson["followActorId"] = m_pendingFollowActorGuid->ToString();
	}
	else
	{
		outJson["followActorId"] = nullptr;
	}

	return true;

}

bool Camera::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object()) return false;

	constexpr int kFollowModeMin		= static_cast<int>(CAMERA_FOLLOW_MODE::FOLLOW_MODE_FIXED);
	constexpr int kFollowModeMax		= static_cast<int>(CAMERA_FOLLOW_MODE::FOLLOW_MODE_TARGET);
	constexpr int kRotationModeMin		= static_cast<int>(CAMERA_ROTATION_MODE::ROTATION_MODE_FIXED);
	constexpr int kRotationModeMax		= static_cast<int>(CAMERA_ROTATION_MODE::ROTATION_MODE_LOOK_AT_TARGET);
	constexpr int kProjectionTypeMin	= static_cast<int>(PROJECTION_TYPE::PROJECTION_TYPE_PERSPECTIVE);
	constexpr int kProjectionTypeMax	= static_cast<int>(PROJECTION_TYPE::PROJECTION_TYPE_ORTHOGRAPHIC);

	// Scheck data existence
	if (!json.contains("name") ||
		!json.contains("followMode") ||
		!json.contains("rotationMode") ||
		!json.contains("rig") ||
		!json.contains("pose") ||
		!json.contains("lens") ||
		!json.contains("targetActorId") ||
		!json.contains("followActorId"))
	{
		return false;
	}

	// Check data types
	if (!json["name"].is_string() ||
		!json["followMode"].is_number_integer() ||
		!json["rotationMode"].is_number_integer() ||
		!json["rig"].is_object() ||
		!json["pose"].is_object() ||
		!json["lens"].is_object())
	{
		return false;
	}

	const auto& rigJson = json["rig"];
	const auto& poseJson = json["pose"];
	const auto& lensJson = json["lens"];

	// Check required fields in rig, pose, and lens
	if (!rigJson.contains("offsetPosition") ||
		!rigJson.contains("offsetRotation") ||
		!poseJson.contains("position") ||
		!poseJson.contains("rotation") ||
		!lensJson.contains("fov") ||
		!lensJson.contains("width") ||
		!lensJson.contains("height") ||
		!lensJson.contains("nearZ") ||
		!lensJson.contains("farZ") ||
		!lensJson.contains("projectionType"))
	{
		return false;
	}

	std::string parsedName;
	int parsedFollowModeValue = 0;
	int parsedRotationModeValue = 0;
	int parsedProjectionTypeValue = 0;

	CameraRig parsedRig;
	CameraPose parsedPose;
	CameraLens parsedLens;

	// Store GUID of referenced actors for later resolution
	std::optional<Guid> parsedTargetGuid;
	std::optional<Guid> parsedFollowGuid;

	try
	{
		parsedName = json["name"].get<std::string>();

		parsedFollowModeValue	 = json["followMode"].get<int>();
		parsedRotationModeValue	 = json["rotationMode"].get<int>();

		parsedLens.fov		= lensJson["fov"].get<float>();
		parsedLens.width	= lensJson["width"].get<float>();
		parsedLens.height	= lensJson["height"].get<float>();
		parsedLens.nearZ	= lensJson["nearZ"].get<float>();
		parsedLens.farZ		= lensJson["farZ"].get<float>();

		parsedProjectionTypeValue = lensJson["projectionType"].get<int>();
	}
	catch (const nlohmann::json::exception&)
	{
		return false;
	}

	// Check if values are convertible
	if (!JsonMath::TryRead(rigJson["offsetPosition"],parsedRig.offsetPosition)	||
		!JsonMath::TryRead(rigJson["offsetRotation"],parsedRig.offsetRotation)	||
		!JsonMath::TryRead(poseJson["position"],parsedPose.position)			||
		!JsonMath::TryRead(poseJson["rotation"],parsedPose.rotation))
	{
		return false;
	}

	// Validate enum ranges
	if (parsedFollowModeValue < kFollowModeMin ||
		parsedFollowModeValue > kFollowModeMax)
	{
		return false;
	}
	if (parsedRotationModeValue < kRotationModeMin ||
		parsedRotationModeValue > kRotationModeMax)
	{
		return false;
	}
	if (parsedProjectionTypeValue < kProjectionTypeMin ||
		parsedProjectionTypeValue > kProjectionTypeMax)
	{
		return false;
	}

	// Validate numeric values for lens parameters
	if (!std::isfinite(parsedLens.fov)		||
		!std::isfinite(parsedLens.width)	||
		!std::isfinite(parsedLens.height)	||
		!std::isfinite(parsedLens.nearZ)	||
		!std::isfinite(parsedLens.farZ))
	{
		return false;
	}
	
	// Validate lens parameters for logical correctness
	if (parsedLens.fov <= 0.0f		||
		parsedLens.width < 0.0f	||
		parsedLens.height < 0.0f	||
		parsedLens.nearZ <= 0.0f	||
		parsedLens.farZ <= parsedLens.nearZ)
	{
		return false;
	}

	// Validate rig and pose parameters for non-zero length
	if (parsedRig.offsetRotation.LengthSq() <= 0.000001f ||
		parsedPose.rotation.LengthSq() <= 0.000001f)
	{
		return false;
	}

	// Assign parsed values to the camera component
	parsedLens.projectionType = static_cast<PROJECTION_TYPE>(parsedProjectionTypeValue);
	parsedRig.offsetRotation = parsedRig.offsetRotation.Normalized();
	parsedPose.rotation = parsedPose.rotation.Normalized();

	// Lambda function to parse actor GUIDs from JSON
	const auto parseActorGuid =
		[](const nlohmann::json& guidJson,
			std::optional<Guid>& outGuid) -> bool
		{
			if (guidJson.is_null())
			{
				outGuid.reset();
				return true;
			}

			if (!guidJson.is_string()) return false;

			Guid parsedGuid;
			if (!Guid::TryParse(
				guidJson.get<std::string>(),
				parsedGuid) ||
				!parsedGuid.IsValid())
			{
				return false;
			}

			outGuid = parsedGuid;
			return true;
		};

	// Parse target and follow actor GUIDs
	if (!parseActorGuid(json["targetActorId"], parsedTargetGuid) ||
		!parseActorGuid(json["followActorId"], parsedFollowGuid))
	{
		return false;
	}

	// Assign parsed values to the camera component
	// Name
	SetName(parsedName);

	// Modes
	m_followMode = static_cast<CAMERA_FOLLOW_MODE>(parsedFollowModeValue);
	m_rotationMode = static_cast<CAMERA_ROTATION_MODE>(parsedRotationModeValue);

	// Rig, Pose, Lens
	m_cameraRig = parsedRig;
	m_cameraPose = parsedPose;
	m_cameraLens = parsedLens;

	// Release any existing references to actors
	m_pTargetActor = nullptr;
	m_pFollowActor = nullptr;

	// Store pending GUIDs for later resolution
	m_pendingTargetActorGuid = parsedTargetGuid;
	m_pendingFollowActorGuid = parsedFollowGuid;

	m_isCameraInfoDirty = true; // Mark camera info as dirty to trigger rebuild
	m_followingTransformGeneration = static_cast<uint64_t>(-1);	// Reset following transform generation
	m_rotatingTransformGeneration = static_cast<uint64_t>(-1);	// Reset rotating transform generation

	return true;
}

bool Camera::ResolveReferences(SceneBase& scene)
{
	Actor* resolvedTargetActor = m_pTargetActor;
	Actor* resolvedFollowActor = m_pFollowActor;

	// Resolve target actor reference
	if (m_pendingTargetActorGuid.has_value())
	{
		resolvedTargetActor = scene.ResolveActor(m_pendingTargetActorGuid.value());
		if (!resolvedTargetActor)
		{
			DBG("Camera::ResolveReferences() - Failed to resolve target actor with GUID: %s\n",
				m_pendingTargetActorGuid->ToString().c_str());
			return false;
		}
	}

	// Resolve follow actor reference
	if (m_pendingFollowActorGuid.has_value())
	{
		resolvedFollowActor = scene.ResolveActor(m_pendingFollowActorGuid.value());
		if (!resolvedFollowActor)
		{
			DBG("Camera::ResolveReferences() - Failed to resolve follow actor with GUID: %s\n",
				m_pendingFollowActorGuid->ToString().c_str());
			return false;
		}
	}

	// Update references and clear pending GUIDs after both resolutions
	m_pTargetActor = resolvedTargetActor;
	m_pFollowActor = resolvedFollowActor;
	m_pendingTargetActorGuid.reset();
	m_pendingFollowActorGuid.reset();

	m_isCameraInfoDirty = true; // Mark camera info as dirty to trigger rebuild
	m_followingTransformGeneration = static_cast<uint64_t>(-1);	// Reset following transform generation
	m_rotatingTransformGeneration = static_cast<uint64_t>(-1);	// Reset rotating transform generation

	return true;
}