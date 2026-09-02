#pragma once
#include "Core/EditorCameraController.h"

//----------------------------------------------------
// EditorViewCamera class
// Facade for the camera owned by one 3D editor view.
//----------------------------------------------------

// Temporary navigation tuning for the editor camera.
struct EditorCameraNavigationState
{
    float moveSpeed = 10.0f;
};

// Facade for the camera owned by one 3D editor view.
class EditorViewCamera
{
public:
    void Initialize(uint32_t width, uint32_t height);

    Camera& GetCamera() { return m_camera; }
    const Camera& GetCamera() const { return m_camera; }
    const Vector3& GetPivot() const { return m_pivot; }
    EditorCameraNavigationState& GetNavigationState() { return m_navigationState; }
    const EditorCameraNavigationState& GetNavigationState() const { return m_navigationState; }

    void Look(const Vector2& deltaPixels, float radiansPerPixel, float maxPitchRadians);
    void Fly(const Vector3& localMovement, float distance);
    void Orbit(const Vector2& deltaPixels, float radiansPerPixel, float maxPitchRadians);
    void Pan(const Vector2& deltaPixels, float distanceScalePerPixel);
    void Dolly(float wheelDelta, float distanceFractionPerStep, float minimumDistance);
    void Focus(const EditorCameraFocusBounds& bounds, float padding, float minimumDistance);

private:
    Camera m_camera;
    Vector3 m_pivot = Vector3::Zero();
    EditorCameraNavigationState m_navigationState;
    EditorCameraController m_controller;
};
