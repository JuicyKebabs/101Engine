#pragma once
#include "Engine/Component/Camera.h"
//----------------------------------------------------------------------
// EditorCamera class
// A Camera subclass used only by EditorApp's free-fly editor camera.
// Not part of any scene file (the Actor that owns this is held outside
//SceneBase by EditorApp, see m_pEditorCameraActor).
//----------------------------------------------------------------------

class EditorCamera : public Camera
{
public:
    EditorCamera() = default;
    ~EditorCamera() = default;

    void UpdateOverride(float deltaTime) override;

    // Editor camera initial setup
    void Initialize(uint32_t width, uint32_t height)
    {
        Camera::ParamDesc desc;
        desc.window_width  = width;
        desc.window_height = height;
        desc.name = "EditorCamera";
        SetParams(desc);

        // Initial position
        CameraPose pose;
        pose.position = Vector3(0.0f, 3.0f, -10.0f);
        pose.rotation = Quaternion::Identity();
        SetCameraPose(pose);

        // We manage position/rotation ourselves via mouse input
        SetFollowMode(CAMERA_FOLLOW_MODE::FOLLOW_MODE_FIXED);
        SetRotationMode(CAMERA_ROTATION_MODE::ROTATION_MODE_FIXED);
    }

private:
    void ProcessMouseInput(float deltaTime);

    float m_yaw   = 0.0f;
    float m_pitch = 0.0f;
    float m_moveSpeed   = 10.0f;
    float m_rotateSpeed = 0.3f;
    float m_zoomSpeed   = 5.0f;

    // Mouse input state
    bool  m_isRightDragging  = false;
    bool  m_isMiddleDragging = false;
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;
};
