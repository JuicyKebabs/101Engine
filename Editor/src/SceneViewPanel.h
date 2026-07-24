#pragma once
#include <d3d12.h>

//--------------------------------------------------------------------------------------------
// SceneViewPanel class
// This class is responsible for rendering the scene view panel in the application.
// Receive GPU handle of the render target and render the scene view panel using ImGui::Imgui.
//--------------------------------------------------------------------------------------------

class SceneViewPanel
{
public:
	void Render(D3D12_GPU_DESCRIPTOR_HANDLE sceneTextureHandle);

	bool IsHovered() const { return m_isHovered; }
	bool IsFocused() const { return m_isFocused; }

private:
	bool m_isHovered = false;
	bool m_isFocused = false;
};