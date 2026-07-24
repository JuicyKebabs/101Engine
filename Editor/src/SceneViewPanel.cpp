#include <algorithm>
#include "SceneViewPanel.h"
#include "imgui.h"

void SceneViewPanel::Render(D3D12_GPU_DESCRIPTOR_HANDLE sceneTextureHandle)
{
	if (ImGui::Begin("Scene"))
	{
		m_isHovered = false;
		m_isFocused = false;

		ImGui::End();
		return;
	}

	m_isHovered = ImGui::IsWindowHovered();
	m_isFocused = ImGui::IsWindowFocused();

	// Get the available size of the content region in the ImGui window
	const ImVec2 availableSize = ImGui::GetContentRegionAvail();

	if (availableSize.x <= 0.0f ||
		availableSize.y <= 0.0f ||
	sceneTextureHandle.ptr == 0)
	{
		ImGui::End();
		return;
	}

	// Resolution of the source texture 
	// (currently fixed at 1280x720, but this could be made dynamic in the future)
	constexpr float sourceAspect = 1280.0f / 720.0f;

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
		reinterpret_cast<ImTextureID>(sceneTextureHandle.ptr),
		{ imageWidth, imageHeight },
		{ 0.0f, 0.0f },
		{ 1.0f, 1.0f });

	ImGui::End();
}