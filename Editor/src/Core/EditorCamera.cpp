#include "EditorCamera.h"
#include "imgui.h"
#include <algorithm>

void EditorCamera::UpdateOverride(float deltaTime)
{
    ProcessMouseInput(deltaTime);
}

void EditorCamera::ProcessMouseInput(float deltaTime)
{
    ImGuiIO& io = ImGui::GetIO();

    // Don't move the camera while ImGui wants the mouse (panels, popups, etc.)
    if (io.WantCaptureMouse) return;

    CameraPose pose = GetCameraPose();

    // Right mouse drag -> rotate (yaw/pitch)
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        if (!m_isRightDragging)
        {
            m_isRightDragging = true;
            m_lastMouseX = io.MousePos.x;
            m_lastMouseY = io.MousePos.y;
        }

        float dx = (io.MousePos.x - m_lastMouseX) * m_rotateSpeed * deltaTime;
        float dy = (io.MousePos.y - m_lastMouseY) * m_rotateSpeed * deltaTime;
        m_lastMouseX = io.MousePos.x;
        m_lastMouseY = io.MousePos.y;

        m_yaw   += dx;
        m_pitch += dy;
        m_pitch = std::clamp(m_pitch, -1.5f, 1.5f);

        pose.rotation = Quaternion::CreateFromEulerRad(
            Vector3(m_pitch, m_yaw, 0.0f)
        );
    }
    else
    {
        m_isRightDragging = false;
    }

    // Middle mouse drag -> pan
    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
    {
        if (!m_isMiddleDragging)
        {
            m_isMiddleDragging = true;
            m_lastMouseX = io.MousePos.x;
            m_lastMouseY = io.MousePos.y;
        }

        float dx = (io.MousePos.x - m_lastMouseX) * m_moveSpeed * deltaTime;
        float dy = (io.MousePos.y - m_lastMouseY) * m_moveSpeed * deltaTime;
        m_lastMouseX = io.MousePos.x;
        m_lastMouseY = io.MousePos.y;

        Vector3 right = pose.rotation.RotateVector3(Vector3::Right());
        Vector3 up    = pose.rotation.RotateVector3(Vector3::Up());
        pose.position.x -= right.x * dx;
        pose.position.y -= right.y * dx;
        pose.position.z -= right.z * dx;
        pose.position.x += up.x * dy;
        pose.position.y += up.y * dy;
        pose.position.z += up.z * dy;
    }
    else
    {
        m_isMiddleDragging = false;
    }

    // Mouse wheel -> zoom (move along forward vector)
    if (io.MouseWheel != 0.0f)
    {
        Vector3 forward = pose.rotation.RotateVector3(Vector3::Forward());
        pose.position.x += forward.x * io.MouseWheel * m_zoomSpeed * deltaTime;
        pose.position.y += forward.y * io.MouseWheel * m_zoomSpeed * deltaTime;
        pose.position.z += forward.z * io.MouseWheel * m_zoomSpeed * deltaTime;
    }

    SetCameraPose(pose);
}
