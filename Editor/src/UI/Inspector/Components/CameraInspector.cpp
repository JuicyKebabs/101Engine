#include "UI/Inspector/Components/CameraInspector.h"
#include <algorithm>
#include <DirectXMath.h>
#include "Engine/Component/Camera.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"

void CameraInspector::Draw(Camera& camera, const InspectorContext&)
{
	int followMode = static_cast<int>(camera.GetFollowMode());
	int rotationMode = static_cast<int>(camera.GetRotationMode());

	CameraRig rig = camera.GetCameraRig();
	Vector3 rigRotation = rig.offsetRotation.ToEulerDeg();

	CameraPose pose = camera.GetCameraPose();
	Vector3 poseRotation = pose.rotation.ToEulerDeg();

	CameraLens lens = camera.GetCameraLens();
	int projectionType = static_cast<int>(lens.projectionType);
	float fov = DirectX::XMConvertToDegrees(lens.fov);

	static const char* followItems[] = {
		"Fixed",
		"Owner",
		"Target"
	};

	static const char* rotationItems[] = {
		"Fixed",
		"Match Owner",
		"Look At Target"
	};

	static const char* projectionItems[] = {
		"Perspective",
		"Orthographic"
	};

	if (!EditorUI::BeginPropertyGrid("CameraProperties")) return;

	if (EditorUI::ComboField("Follow Mode", followMode, followItems, 3))
	{
		camera.SetFollowMode(static_cast<CAMERA_FOLLOW_MODE>(followMode));
	}

	if (EditorUI::ComboField("Rotation Mode", rotationMode, rotationItems, 3))
	{
		camera.SetRotationMode(static_cast<CAMERA_ROTATION_MODE>(rotationMode));
	}

	bool rigChanged = false;
	rigChanged |= EditorUI::Vector3Field("Rig Position", rig.offsetPosition);
	rigChanged |= EditorUI::Vector3Field("Rig Rotation", rigRotation, 0.5f);

	if (rigChanged)
	{
		rig.offsetRotation = Quaternion::CreateFromEulerDeg(rigRotation);
		camera.SetCameraRig(rig);
	}

	bool poseChanged = false;
	poseChanged |= EditorUI::Vector3Field("Pose Position", pose.position);
	poseChanged |= EditorUI::Vector3Field("Pose Rotation", poseRotation, 0.5f);

	if (poseChanged)
	{
		pose.rotation = Quaternion::CreateFromEulerDeg(poseRotation);
		camera.SetCameraPose(pose);
	}

	bool lensChanged = false;
	lensChanged |= EditorUI::ComboField("Projection", projectionType, projectionItems, 2);
	lensChanged |= EditorUI::FloatField("FOV", fov, 0.5f);
	lensChanged |= EditorUI::FloatField("Width", lens.width, 1.0f);
	lensChanged |= EditorUI::FloatField("Height", lens.height, 1.0f);
	lensChanged |= EditorUI::FloatField("Near", lens.nearZ, 0.01f);
	lensChanged |= EditorUI::FloatField("Far", lens.farZ, 1.0f);

	if (lensChanged)
	{
		lens.projectionType = static_cast<PROJECTION_TYPE>(projectionType);
		lens.fov = DirectX::XMConvertToRadians(std::clamp(fov, 1.0f, 179.0f));
		lens.width = std::max(lens.width, 1.0f);
		lens.height = std::max(lens.height, 1.0f);
		lens.nearZ = std::max(lens.nearZ, 0.001f);
		lens.farZ = std::max(lens.farZ, lens.nearZ + 0.001f);
		camera.SetCameraLens(lens);
	}

	EditorUI::EndPropertyGrid();
}
