#include "EditorCameraController.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kDirectionEpsilonSq = 1.0e-8f;

    Quaternion BuildYawPitchRotation(
        const Quaternion& currentRotation,
        const Vector2& deltaPixels,
        float radiansPerPixel,
        float maxPitchRadians)
    {
        const Vector3 forward = currentRotation.RotateVector3(Vector3::Forward()).Normalized();

        float yaw = std::atan2(forward.x, forward.z);
        float pitch = std::asin(std::clamp(-forward.y, -1.0f, 1.0f));

        yaw += deltaPixels.x * radiansPerPixel;
        pitch += deltaPixels.y * radiansPerPixel;
        pitch = std::clamp(pitch, -maxPitchRadians, maxPitchRadians);

        return Quaternion::CreateFromEulerRad({ pitch, yaw, 0.0f }).Normalized();
    }
}

void EditorCameraController::Look(
    Camera& camera,
    Vector3& pivot,
    const Vector2& deltaPixels,
    float radiansPerPixel,
    float maxPitchRadians) const
{
    CameraPose pose = camera.GetCameraPose();
    const float pivotDistance = (pivot - pose.position).Length();

    pose.rotation = BuildYawPitchRotation(
        pose.rotation,
        deltaPixels,
        radiansPerPixel,
        maxPitchRadians);

    pivot = pose.position +
        pose.rotation.RotateVector3(Vector3::Forward()) * pivotDistance;
    camera.SetCameraPose(pose);
}

void EditorCameraController::Fly(
    Camera& camera,
    Vector3& pivot,
    const Vector3& localMovement,
    float distance) const
{
    if (localMovement.LengthSq() <= kDirectionEpsilonSq || distance == 0.0f) return;

    CameraPose pose = camera.GetCameraPose();
    const Vector3 forward = pose.rotation.RotateVector3(Vector3::Forward());
    const Vector3 right = pose.rotation.RotateVector3(Vector3::Right());

    Vector3 direction =
        right * localMovement.x +
        Vector3::Up() * localMovement.y +
        forward * localMovement.z;

    if (direction.LengthSq() <= kDirectionEpsilonSq) return;

    const Vector3 translation = direction.Normalized() * distance;
    pose.position += translation;
    pivot += translation;
    camera.SetCameraPose(pose);
}

void EditorCameraController::Orbit(
    Camera& camera,
    const Vector3& pivot,
    const Vector2& deltaPixels,
    float radiansPerPixel,
    float maxPitchRadians) const
{
    CameraPose pose = camera.GetCameraPose();
    const float pivotDistance = (pivot - pose.position).Length();

    pose.rotation = BuildYawPitchRotation(
        pose.rotation,
        deltaPixels,
        radiansPerPixel,
        maxPitchRadians);

    const Vector3 forward = pose.rotation.RotateVector3(Vector3::Forward());
    pose.position = pivot - forward * pivotDistance;
    camera.SetCameraPose(pose);
}

void EditorCameraController::Pan(
    Camera& camera,
    Vector3& pivot,
    const Vector2& deltaPixels,
    float distanceScalePerPixel) const
{
    CameraPose pose = camera.GetCameraPose();
    const float pivotDistance = (pivot - pose.position).Length();
    const float unitsPerPixel = pivotDistance * distanceScalePerPixel;
    const Vector3 right = pose.rotation.RotateVector3(Vector3::Right());
    const Vector3 up = pose.rotation.RotateVector3(Vector3::Up());
    const Vector3 translation =
        right * (-deltaPixels.x * unitsPerPixel) +
        up * (deltaPixels.y * unitsPerPixel);

    pose.position += translation;
    pivot += translation;
    camera.SetCameraPose(pose);
}

void EditorCameraController::Dolly(
    Camera& camera,
    const Vector3& pivot,
    float wheelDelta,
    float distanceFractionPerStep,
    float minimumDistance) const
{
    if (wheelDelta == 0.0f) return;

    CameraPose pose = camera.GetCameraPose();
    const Vector3 toPivot = pivot - pose.position;
    const float currentDistance = toPivot.Length();
    if (currentDistance <= kDirectionEpsilonSq) return;

    const float nextDistance = std::max(
        minimumDistance,
        currentDistance * (1.0f - wheelDelta * distanceFractionPerStep));

    pose.position = pivot - toPivot.Normalized() * nextDistance;
    camera.SetCameraPose(pose);
}

void EditorCameraController::Focus(
    Camera& camera,
    Vector3& pivot,
    const EditorCameraFocusBounds& bounds,
    float padding,
    float minimumDistance) const
{
    CameraPose pose = camera.GetCameraPose();
    const CameraLens lens = camera.GetCameraLens();
    const float aspect = lens.height > 0.0f ? lens.width / lens.height : 1.0f;
    const float verticalHalfFov = lens.fov * 0.5f;
    const float horizontalHalfFov = std::atan(std::tan(verticalHalfFov) * aspect);
    const float limitingHalfFov = std::min(verticalHalfFov, horizontalHalfFov);
    const float safeRadius = std::max(0.0f, bounds.radius);
    const float fitDistance = safeRadius > 0.0f
        ? safeRadius / std::max(std::sin(limitingHalfFov), 0.001f) * padding
        : minimumDistance;
    const float distance = std::max(minimumDistance, fitDistance);
    const Vector3 forward = pose.rotation.RotateVector3(Vector3::Forward()).Normalized();

    pivot = bounds.center;
    pose.position = pivot - forward * distance;
    camera.SetCameraPose(pose);
}
