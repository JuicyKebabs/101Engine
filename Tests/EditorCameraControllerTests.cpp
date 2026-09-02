#include <cmath>
#include <cstdlib>
#include <iostream>
#include <type_traits>

#include "Core/EditorViewCamera.h"

namespace
{
    int failures = 0;

    void Check(bool condition, const char* message)
    {
        if (condition) return;
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }

    bool Near(float lhs, float rhs, float epsilon = 0.001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    bool Near(const Vector3& lhs, const Vector3& rhs, float epsilon = 0.001f)
    {
        return lhs.NearEqual(rhs, epsilon);
    }

    void CheckPivotOnForward(const EditorViewCamera& view, const char* message)
    {
        const CameraPose pose = view.GetCamera().GetCameraPose();
        const Vector3 toPivot = view.GetPivot() - pose.position;
        const Vector3 forward = pose.rotation.RotateVector3(Vector3::Forward());
        Check(toPivot.Normalized().NearEqual(forward.Normalized(), 0.001f), message);
    }
}

int main()
{
    static_assert(std::is_empty_v<EditorCameraController>);

    EditorViewCamera view;
    view.Initialize(1280, 720);
    CheckPivotOnForward(view, "initial pivot is on camera forward");

    {
        const CameraPose before = view.GetCamera().GetCameraPose();
        const float distanceBefore = (view.GetPivot() - before.position).Length();
        view.Look({ 40.0f, -20.0f }, 0.003f, PI_DIV_2 - 0.01f);
        const CameraPose after = view.GetCamera().GetCameraPose();

        Check(Near(before.position, after.position), "look keeps camera position fixed");
        Check(Near(distanceBefore, (view.GetPivot() - after.position).Length()),
            "look preserves pivot distance");
        CheckPivotOnForward(view, "look places pivot on the new forward");
    }

    {
        const CameraPose before = view.GetCamera().GetCameraPose();
        const Vector3 pivotBefore = view.GetPivot();
        view.Fly({ 1.0f, 1.0f, 1.0f }, 2.0f);
        const CameraPose after = view.GetCamera().GetCameraPose();

        Check(Near(after.position - before.position, view.GetPivot() - pivotBefore),
            "fly translates camera and pivot equally");
        Check(Near((after.position - before.position).Length(), 2.0f),
            "fly normalizes diagonal input");
    }

    {
        const CameraPose before = view.GetCamera().GetCameraPose();
        const Vector3 pivotBefore = view.GetPivot();
        view.Pan({ 15.0f, -8.0f }, 0.002f);
        const CameraPose after = view.GetCamera().GetCameraPose();

        Check(Near(after.position - before.position, view.GetPivot() - pivotBefore),
            "pan translates camera and pivot equally");
        Check(after.rotation.NearEqual(before.rotation), "pan keeps camera rotation fixed");
        CheckPivotOnForward(view, "pan preserves the pivot-forward relationship");
    }

    {
        const Vector3 pivotBefore = view.GetPivot();
        const CameraPose before = view.GetCamera().GetCameraPose();
        const float distanceBefore = (pivotBefore - before.position).Length();
        view.Orbit({ -30.0f, 12.0f }, 0.003f, PI_DIV_2 - 0.01f);
        const CameraPose after = view.GetCamera().GetCameraPose();

        Check(Near(view.GetPivot(), pivotBefore), "orbit keeps pivot fixed");
        Check(Near((view.GetPivot() - after.position).Length(), distanceBefore),
            "orbit preserves pivot distance");
        CheckPivotOnForward(view, "orbit keeps camera aimed at pivot");
    }

    {
        const Vector3 pivotBefore = view.GetPivot();
        view.Dolly(100.0f, 0.12f, 0.25f);
        const CameraPose after = view.GetCamera().GetCameraPose();

        Check(Near(view.GetPivot(), pivotBefore), "dolly keeps pivot fixed");
        Check(Near((view.GetPivot() - after.position).Length(), 0.25f),
            "dolly clamps at the minimum distance");
        CheckPivotOnForward(view, "dolly cannot pass through pivot");
    }

    {
        const Quaternion rotationBefore = view.GetCamera().GetCameraPose().rotation;
        const EditorCameraFocusBounds bounds{ { 4.0f, 2.0f, -3.0f }, 2.0f };
        view.Focus(bounds, 1.2f, 0.1f);

        Check(Near(view.GetPivot(), bounds.center), "focus moves pivot to bounds center");
        Check(view.GetCamera().GetCameraPose().rotation.NearEqual(rotationBefore),
            "focus keeps the current view direction");
        Check((view.GetPivot() - view.GetCamera().GetCameraPose().position).Length() > bounds.radius,
            "focus places camera far enough to frame bounds");
        CheckPivotOnForward(view, "focus keeps pivot on camera forward");
    }

    if (failures == 0)
    {
        std::cout << "EditorCameraControllerTests passed\n";
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
