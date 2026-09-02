#pragma once
#include "Engine/Component/Camera.h"

//----------------------------------------------------------------------------
// EditorCameraController class
// Camera navigation math only. Input devices, selection, and editor settings
// are intentionally owned by callers outside this class.
//----------------------------------------------------------------------------

// Struct to hold the bounds for focusing the editor camera on a specific area or object in the scene.
struct EditorCameraFocusBounds
{
    Vector3 center = Vector3::Zero();
    float radius = 0.0f;
};

// Camera navigation math only. Input devices, selection, and editor settings
// are intentionally owned by callers outside this class.
class EditorCameraController
{
public:
    void Look(
        Camera& camera,
        Vector3& pivot,
        const Vector2& deltaPixels,
        float radiansPerPixel,
        float maxPitchRadians
    ) const;

    void Fly(
        Camera& camera,
        Vector3& pivot,
        const Vector3& localMovement,
        float distance
    ) const;

    void Orbit(
        Camera& camera,
        const Vector3& pivot,
        const Vector2& deltaPixels,
        float radiansPerPixel,
        float maxPitchRadians
    ) const;

    void Pan(
        Camera& camera,
        Vector3& pivot,
        const Vector2& deltaPixels,
        float distanceScalePerPixel
    ) const;

    void Dolly(
        Camera& camera,
        const Vector3& pivot,
        float wheelDelta,
        float distanceFractionPerStep,
        float minimumDistance
    ) const;

    void Focus(
        Camera& camera,
        Vector3& pivot,
        const EditorCameraFocusBounds& bounds,
        float padding,
        float minimumDistance
    ) const;
};
