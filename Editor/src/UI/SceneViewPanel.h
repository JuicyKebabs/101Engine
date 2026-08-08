#pragma once
#include <d3d12.h>
#include "imgui.h"
#include "Engine/Core/Math/Math.h"

//--------------------------------------------------------------------------------------------
// SceneViewPanel class
// This class is responsible for rendering the scene view panel in the application.
// Receive GPU handle of the render target and render the scene view panel using ImGui::Imgui.
//--------------------------------------------------------------------------------------------

// Enumration to define the editor viewport mode
enum class EditorViewportMode
{
	Scene,
	Screen,
};

class SceneViewPanel
{
public:
	void Render(
		D3D12_GPU_DESCRIPTOR_HANDLE sceneTextureHandle,
		UINT textureWidth, UINT textureHeight
	);

	// Consume a resize request from the scene view panel and return the new width and height
	bool ConsumeResizeRequest(UINT& outWidth, UINT& outHeight);

	// Consume a pick request and return the viewport UV coordinates of the mouse click
	bool ConsumePickRequest(Vector2& outViewportUV);

	EditorViewportMode GetViewMode() const { return m_viewMode; }

	bool IsHovered() const { return m_isHovered; }
	bool IsFocused() const { return m_isFocused; }

	ImVec2 GetViewportSize() const { return m_viewportSize; }
	ImVec2 GetImageMin() const { return m_imageMin; }
	ImVec2 GetImageMax() const { return m_imageMax; }

private:
	static constexpr float kRenderScale = 1.0f; // Scale factor for rendering the scene view panel

	// Current editor viewport mode (Scene or Screen)
	EditorViewportMode m_viewMode = EditorViewportMode::Scene;

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

	// Store the UV coordinates of the mouse click in the viewport for picking actors
	Vector2 m_pickUV = Vector2::Zero();

	// Flag to indicate if a pick request has been made (mouse click in the viewport)
	bool m_hasPickRequest = false;
};
