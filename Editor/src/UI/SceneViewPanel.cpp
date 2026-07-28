#include <algorithm>
#include <cmath>
#include "SceneViewPanel.h"
#include "imgui.h"

void SceneViewPanel::Render(
	D3D12_GPU_DESCRIPTOR_HANDLE sceneTextureHandle,
	UINT textureWidth, UINT textureHeight
)
{
	if (!ImGui::Begin("Scene"))
	{
		m_isHovered = false;
		m_isFocused = false;

		ImGui::End();
		return;
	}

	m_isHovered = ImGui::IsWindowHovered();
	m_isFocused = ImGui::IsWindowFocused();

	// Display the resolution of the render target
	ImGui::Text(
		"Scene View Resolution: %u x %u",
		textureWidth,
		textureHeight
	);

	// Get the available size of the content region in the ImGui window
	const ImVec2 availableSize = ImGui::GetContentRegionAvail();

	if (availableSize.x <= 0.0f		||
		availableSize.y <= 0.0f		||
		sceneTextureHandle.ptr == 0	||
		textureWidth == 0			||
		textureHeight == 0
		)
	{
		ImGui::End();
		return;
	}

	// Calculate the requested width and height of the viewport based on the available size
	const UINT requestedWidth = static_cast<UINT>(std::max(1.0f, std::floor(availableSize.x * kRenderScale)));
	const UINT requestedHeight = static_cast<UINT>(std::max(1.0f, std::floor(availableSize.y * kRenderScale)));
	
	// Store the previous width and height of the viewport for comparison
	const UINT previousWidth = static_cast<UINT>(m_viewportSize.x);
	const UINT previousHeight = static_cast<UINT>(m_viewportSize.y);

	if (requestedWidth != previousWidth || requestedHeight != previousHeight)
	{
		m_viewportSize = {static_cast<float>(requestedWidth), static_cast<float>(requestedHeight)};
		m_isViewportResized = true;
	}

	// Resolution of the source texture 
	const float sourceAspect = static_cast<float>(textureWidth) / static_cast<float>(textureHeight);

	// Calculate the size of the image based on width and the aspect ratio of the source texture
	float imageWidth = availableSize.x;
	float imageHeight = imageWidth / sourceAspect;

	// if the hight is greater than the available size Y,
	// we need to scale down the image to fit within the available size based on vertical size
	if (imageHeight > availableSize.y)
	{
		imageHeight = availableSize.y;
		imageWidth = imageHeight * sourceAspect;
	}

	// Calculate the offset to center the image within the available space
	const float offsetX = std::max(0.0f, (availableSize.x - imageWidth) * 0.5f);
	const float offsetY = std::max(0.0f, (availableSize.y - imageHeight) * 0.5f);

	// Set the cursor position to the calculated offset position
	const ImVec2 cursorPosition = ImGui::GetCursorPos();

	ImGui::SetCursorPos({
		cursorPosition.x + offsetX,
		cursorPosition.y + offsetY
		});

	// Render the image using the ImGui::Image function, passing in the texture handle and the calculated size
	ImGui::Image(
		static_cast<ImTextureID>(sceneTextureHandle.ptr),
		{ imageWidth, imageHeight },
		{ 0.0f, 0.0f },
		{ 1.0f, 1.0f });

	// Store the minimum and maximum coordinates of the image in the ImGui window for later use
	m_imageMin = ImGui::GetItemRectMin();
	m_imageMax = ImGui::GetItemRectMax();

	ImGui::End();
}

bool SceneViewPanel::ConsumeResizeRequest(UINT& outWidth, UINT& outHeight)
{
	if (!m_isViewportResized) return false;

	const UINT width = static_cast<UINT>(m_viewportSize.x);
	const UINT height = static_cast<UINT>(m_viewportSize.y);

	if (width == 0 || height == 0)
	{
		m_isViewportResized = false;
		return false;
	}

	outWidth = width;
	outHeight = height;

	m_isViewportResized = false;
	return true;
}