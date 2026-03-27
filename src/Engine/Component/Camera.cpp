#include "Camera.h"
#include "Transform.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Debug/Debug.h"

using namespace DirectX;

Camera::Camera(float window_width, float window_height, const std::string& name)
	: Component(name)
{
	m_cameraLens.width = window_width;
	m_cameraLens.height = window_height;
}

void Camera::OnStart()
{
}

void Camera::PreUpdate(float deltaTime)
{
}

void Camera::Update(float deltaTime)
{
}

void Camera::LateUpdate(float deltaTime)
{
}

void Camera::OnDestroy()
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

	// Build view matrix
	Matrix4x4 view = Matrix4x4::Identity;
	// Apply rotation (note the inverse order for view matrix)
	Matrix4x4 rotationMatrix = m_cameraPose.rotation.ToRotationMatrix();
	view *= rotationMatrix;
	view *= Matrix4x4::CreateTranslation(-m_cameraPose.position); // Apply inverse translation for view matrix
	info.viewMatrix = view.Inverse();

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
		auto followTransform = followingTarget->GetTransform();
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
		auto rotatingTransform = rotatingTarget->GetTransform();
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

	auto targetTransform = followTarget->GetTransform();
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
			auto ownerTransform = GetOwner()->GetTransform();
			if (ownerTransform) {
				targetRotation = ownerTransform->GetWorldRotationQuat();
				// Apply offset rotation from the camera rig
				targetRotation *= m_cameraRig.offsetRotation;
			}
		}
		break;

	case CAMERA_ROTATION_MODE::ROTATION_MODE_LOOK_AT_TARGET:
		if (m_pTargetActor) {
			auto targetTransform = m_pTargetActor->GetTransform();
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