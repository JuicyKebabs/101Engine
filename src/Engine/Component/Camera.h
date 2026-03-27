#pragma once
#include <DirectXMath.h>
#include "Component.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Utility/SharedStruct.h"

class Actor;

//camera information structure
struct CameraInfo
{
	Vector3 position;		//camera position
	Vector3 forward;		//camera forward direction
	Vector3 up;				//camera up direction
	Matrix4x4 viewMatrix;	//view matrix
	Matrix4x4 projMatrix;	//projection matrix
};

// Projection type enumeration
enum class PROJECTION_TYPE
{
	PROJECTION_TYPE_PERSPECTIVE,	// Perspective projection
	PROJECTION_TYPE_ORTHOGRAPHIC	// Orthographic projection
};

// Camera follow mode enumeration
enum class CAMERA_FOLLOW_MODE
{
	FOLLOW_MODE_FIXED,		// Fixed position
	FOLLOW_MODE_OWNER,		// Follow the owning actor
	FOLLOW_MODE_TARGET,		// Follow a specified target
};

// Camera rotation mode enumeration
enum class CAMERA_ROTATION_MODE
{
	ROTATION_MODE_FIXED,			// Fixed rotation
	ROTATION_MODE_MATCH_OWNER,		// Rotate with the owning actor
	ROTATION_MODE_LOOK_AT_TARGET,	// Rotate to look at a specified target
};

// Camera information structure
struct CameraRig
{
	Vector3 offsetPosition = Vector3::Zero();			// Offset position from the target
	Quaternion offsetRotation = Quaternion::Identity();	// Offset rotation from the target
	//float armLength = 1.0f;							// Length of the camera arm (distance from the target)
	//float positionLag = 0.0f;							// Lag for position movement (0 for no lag)
	//float rotationLag = 0.0f;							// Lag for rotation movement (0 for no lag)
	//bool collisionDetection = false;					// Flag to enable/disable collision detection for the camera
	//float collisionRadius = 0.0f;						// Radius for collision detection (if enabled)
};

//Camera pose structure (position and rotation)
struct CameraPose
{
	Vector3 position = Vector3::Zero();				// Camera position
	Quaternion rotation = Quaternion::Identity();	// Camera rotation
};

// Camera information structure (view/projection matrices, etc.)
struct CameraLens
{
	float fov = DirectX::XM_PIDIV2 * 1.1f;											// Vertical field of view (in radians)
	float width = 0.0f;																// screen width
	float height = 0.0f;															// screen height
	float nearZ = 0.1f;																// Near clipping plane distance
	float farZ = 150.0f;															// Far clipping plane distance
	PROJECTION_TYPE projectionType = PROJECTION_TYPE::PROJECTION_TYPE_PERSPECTIVE;	// Projection type (perspective or orthographic)
};

// Camera component Class
class Camera : public Component
{
public:
	Camera(		// Constructor
		float window_width = 0.0f, 
		float window_height = 0.0f,
		const std::string& name = "Camera"
	);
	~Camera() {};	// Destructor

	void OnStart() override;
	void PreUpdate(float deltaTime) override;
	void Update(float deltaTime) override;
	void LateUpdate(float deltaTime) override;
	void OnDestroy() override;
	void Flush(float deltaTime);

	const CameraInfo& GetCameraInfo();	// Build camera information (calculate view/projection matrices, etc.)

	void SetFollowMode(CAMERA_FOLLOW_MODE mode) { m_followMode = mode; m_isCameraInfoDirty = true; }		// Set follow mode
	void SetRotationMode(CAMERA_ROTATION_MODE mode) { m_rotationMode = mode; m_isCameraInfoDirty = true; }	// Set rotation mode
	void SetCameraRig(const CameraRig& rig) { m_cameraRig = rig; m_isCameraInfoDirty = true; }				// Set camera rig settings
	void SetCameraLens(const CameraLens& lens) { m_cameraLens = lens; m_isCameraInfoDirty = true; }			// Set camera lens settings
	void SetCameraPose(const CameraPose& pose) { m_cameraPose = pose; m_isCameraInfoDirty = true; }			// Set camera camera pose (position and rotation)
	void SetTargetActor(Actor* target) { m_pTargetActor = target; m_isCameraInfoDirty = true; }				// Set target actor for follow/rotation (if applicable)
	void SetFollowTarget(Actor* target) { m_pFollowActor = target; m_isCameraInfoDirty = true; }			// Set follow target actor (if applicable)

	CAMERA_FOLLOW_MODE GetFollowMode() const { return m_followMode; }		// Get follow mode
	CAMERA_ROTATION_MODE GetRotationMode() const { return m_rotationMode; }	// Get rotation mode
	CameraRig GetCameraRig() const { return m_cameraRig; }					// Get camera rig settings
	CameraPose GetCameraPose() const { return m_cameraPose; }				// Get camera pose (position and rotation)
	CameraLens GetCameraLens() const { return m_cameraLens; }				// Get camera lens settings

	void SetAsMainCamera();	// Set this camera as the main camera in the scene (if applicable)

private:
	CAMERA_FOLLOW_MODE m_followMode = CAMERA_FOLLOW_MODE::FOLLOW_MODE_OWNER;				// Follow mode (default: follow owner)
	CAMERA_ROTATION_MODE m_rotationMode = CAMERA_ROTATION_MODE::ROTATION_MODE_MATCH_OWNER;	// Rotation mode (default: rotate with owner)

	CameraRig m_cameraRig;		// Camera rig settings
	CameraPose m_cameraPose;	// Camera pose (position and rotation)
	CameraLens m_cameraLens;	// Camera lens settings (projection parameters)

	CameraInfo m_cameraInfo;	// Cached camera information

	Actor* m_pTargetActor = nullptr;	// Target actor for follow/rotation (if applicable)
	Actor* m_pFollowActor = nullptr;	// Actor to follow (if applicable)

	bool m_isCameraInfoDirty = true;				// Flag to indicate if camera information needs to be rebuilt
	uint64_t m_followingTransformGeneration = -1;	// Generation of the transform component that the camera is currently following(used to detect changes in the transform)
	uint64_t m_rotatingTransformGeneration = -1;	// Generation of the transform component that the camera is currently rotating with(used to detect changes in the transform)

private:
	CameraInfo RebuildCameraInfo();			// Rebuild camera information
	void UpdateCameraPose(float deltaTime);	// Update camera pose based on follow mode, rotation mode, and rig settings
	void UpdatePosition(float deltaTime);	// Update camera position based on follow mode and rig settings
	void UpdateRotation(float deltaTime);	// Update camera rotation based on rotation mode and rig settings
};