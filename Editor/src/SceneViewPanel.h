#pragma once
#include <d3d12.h>
#include "imgui.h"

//--------------------------------------------------------------------------------------------
// SceneViewPanel class
// This class is responsible for rendering the scene view panel in the application.
// Receive GPU handle of the render target and render the scene view panel using ImGui::Imgui.
//--------------------------------------------------------------------------------------------

class SceneViewPanel
{
public:
	void Render(
		D3D12_GPU_DESCRIPTOR_HANDLE sceneTextureHandle,
		UINT textureWidth, UINT textureHeight
	);

	bool ConsumeResizeRequest(UINT& outWidth, UINT& outHeight);

	bool IsHovered() const { return m_isHovered; }
	bool IsFocused() const { return m_isFocused; }

	ImVec2 GetViewportSize() const { return m_viewportSize; }
	ImVec2 GetImageMin() const { return m_imageMin; }
	ImVec2 GetImageMax() const { return m_imageMax; }

private:
	bool m_isHovered = false;
	bool m_isFocused = false;

	// Used for resizing the render target when the viewport size changes
	// 1 frame delay is happen, but it is acceptable for editor viewport.
	ImVec2 m_viewportSize = { 0.0f, 0.0f };	

	// Min and max coordinates of the image in the ImGui window, 
	// used for calculating mouse position relative to the image
	ImVec2 m_imageMin = { 0.0f, 0.0f };
	ImVec2 m_imageMax = { 0.0f, 0.0f };

	// Flag to indicate if the viewport size has changed, 
	// triggering a resize of the render target
	bool m_isViewportResized = false;	
};
