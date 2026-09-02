#include "EditorViewCamera.h"

void EditorViewCamera::Initialize(uint32_t width, uint32_t height)
{
    Camera::ParamDesc desc;
    desc.window_width = width;
    desc.window_height = height;
    desc.name = "EditorViewCamera";
    m_camera.SetParams(desc);

    CameraPose pose;
    pose.position = { 0.0f, 3.0f, -10.0f };
    pose.rotation = Quaternion::LookRotation(m_pivot - pose.position, Vector3::Up());
    m_camera.SetCameraPose(pose);
    m_camera.SetFollowMode(CAMERA_FOLLOW_MODE::FOLLOW_MODE_FIXED);
    m_camera.SetRotationMode(CAMERA_ROTATION_MODE::ROTATION_MODE_FIXED);
}

void EditorViewCamera::Look(
    const Vector2& deltaPixels,
    float radiansPerPixel,
    float maxPitchRadians)
{
    m_controller.Look(m_camera, m_pivot, deltaPixels, radiansPerPixel, maxPitchRadians);
}

void EditorViewCamera::Fly(const Vector3& localMovement, float distance)
{
    m_controller.Fly(m_camera, m_pivot, localMovement, distance);
}

void EditorViewCamera::Orbit(
    const Vector2& deltaPixels,
    float radiansPerPixel,
    float maxPitchRadians)
{
    m_controller.Orbit(m_camera, m_pivot, deltaPixels, radiansPerPixel, maxPitchRadians);
}

void EditorViewCamera::Pan(
    const Vector2& deltaPixels,
    float distanceScalePerPixel)
{
    m_controller.Pan(m_camera, m_pivot, deltaPixels, distanceScalePerPixel);
}

void EditorViewCamera::Dolly(
    float wheelDelta,
    float distanceFractionPerStep,
    float minimumDistance)
{
    m_controller.Dolly(
        m_camera,
        m_pivot,
        wheelDelta,
        distanceFractionPerStep,
        minimumDistance);
}

void EditorViewCamera::Focus(
    const EditorCameraFocusBounds& bounds,
    float padding,
    float minimumDistance)
{
    m_controller.Focus(m_camera, m_pivot, bounds, padding, minimumDistance);
}
