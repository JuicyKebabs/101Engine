#include <algorithm>
#include <cmath>
#include "SceneViewPanel.h"
#include "imgui.h"

void SceneViewPanel::Render(
	D3D12_GPU_DESCRIPTOR_HANDLE sceneTextureHandle,
	UINT textureWidth, UINT textureHeight,
	const ViewportOverlayData& overlayData
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

	// Display the reference size and canvas order if in Canvas view mode and a canvas is present
	if (m_viewMode == EditorViewportMode::Canvas && !overlayData.canvasRects.empty())
	{
		ImGui::SameLine();

		ImGui::TextDisabled(
			"Reference: %.0f x %.0f  Order: %u",
			overlayData.referenceSize.x,
			overlayData.referenceSize.y,
			overlayData.canvasOrder
		);
	}

	// Render the breadcrumb navigation for the canvas view mode
	if (m_viewMode == EditorViewportMode::Canvas && !overlayData.canvasBreadcrumbs.empty())
	{
		ImGui::TextDisabled("Canvas Hierarchy:");
		ImGui::SameLine();

		for (size_t i = 0; i < overlayData.canvasBreadcrumbs.size(); i++)
		{
			const CanvasBreadcrumbItem& item = overlayData.canvasBreadcrumbs[i];

			const bool isCurrent = i + 1 == overlayData.canvasBreadcrumbs.size();

			ImGui::PushID(static_cast<int>(i));

			if (item.type == CanvasBreadcrumbItemType::Ellipsis)
			{
				ImGui::TextDisabled("...");
			}
			else if (isCurrent)
			{// Display the current canvas name as unformatted text (not clickable)
				ImGui::TextUnformatted(item.name.c_str());
			}
			else if (ImGui::SmallButton(item.name.c_str()))
			{
				m_canvasOpenRequest = item.canvasActorGuid;
			}

			ImGui::PopID();

			if (!isCurrent)
			{
				ImGui::SameLine();
				ImGui::TextDisabled(">");
				ImGui::SameLine();
			}
		}
	}

	// Button to fit the view to rectangle of the Editing-Root Canvas in Canvas View
	if (m_viewMode == EditorViewportMode::Canvas)
	{
		ImGui::SameLine();

		if (ImGui::SmallButton("Fit"))
		{
			m_canvasNavigationInput.fitRequested = true;
			m_hasCanvasNavigationInput = true;
		}
	}

	// Render the tab bar for switching between Scene and Canvas view modes
	if (ImGui::BeginTabBar("##EditorViewportTabs"))
	{
		if (ImGui::BeginTabItem("Scene"))
		{
			m_viewMode = EditorViewportMode::Scene;
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Canvas"))
		{
			m_viewMode = EditorViewportMode::Canvas;
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

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

	// Mouse input detection in Canvas View
	if (m_viewMode == EditorViewportMode::Canvas && ImGui::IsItemHovered())
	{
		ImGuiIO& io = ImGui::GetIO();

		const float imageWidth = m_imageMax.x - m_imageMin.x;
		const float imageHeight = m_imageMax.y - m_imageMin.y;

		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle,0.0f))
		{
			m_canvasNavigationInput.panDeltaPixels.x += io.MouseDelta.x;
			m_canvasNavigationInput.panDeltaPixels.y += io.MouseDelta.y;

			m_hasCanvasNavigationInput = true;
		}

		if (io.MouseWheel != 0.0f && imageWidth > 0.0f && imageHeight > 0.0f)
		{
			const ImVec2 mousePosition = ImGui::GetMousePos();

			m_canvasNavigationInput.zoomPivotUV =
			{
				(mousePosition.x - m_imageMin.x) / imageWidth,
				(mousePosition.y - m_imageMin.y) / imageHeight
			};

			m_canvasNavigationInput.wheelDelta += io.MouseWheel;

			m_hasCanvasNavigationInput = true;
		}
	}
	else if (m_viewMode == EditorViewportMode::Scene && ImGui::IsItemHovered())
	{
		ImGuiIO& io = ImGui::GetIO();

		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			m_sceneNavigationInput.lookDeltaPixels +=
				Vector2(io.MouseDelta.x, io.MouseDelta.y);
			m_sceneNavigationInput.flyDirection.x =
				(ImGui::IsKeyDown(ImGuiKey_D) ? 1.0f : 0.0f) -
				(ImGui::IsKeyDown(ImGuiKey_A) ? 1.0f : 0.0f);
			m_sceneNavigationInput.flyDirection.y =
				(ImGui::IsKeyDown(ImGuiKey_E) ? 1.0f : 0.0f) -
				(ImGui::IsKeyDown(ImGuiKey_Q) ? 1.0f : 0.0f);
			m_sceneNavigationInput.flyDirection.z =
				(ImGui::IsKeyDown(ImGuiKey_W) ? 1.0f : 0.0f) -
				(ImGui::IsKeyDown(ImGuiKey_S) ? 1.0f : 0.0f);
			m_sceneNavigationInput.fastMove = io.KeyShift;
			m_hasSceneNavigationInput = true;
		}

		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
		{
			m_sceneNavigationInput.panDeltaPixels +=
				Vector2(io.MouseDelta.x, io.MouseDelta.y);
			m_hasSceneNavigationInput = true;
		}

		if (io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
		{
			m_sceneNavigationInput.orbitDeltaPixels +=
				Vector2(io.MouseDelta.x, io.MouseDelta.y);
			m_hasSceneNavigationInput = true;
		}

		if (io.MouseWheel != 0.0f)
		{
			m_sceneNavigationInput.wheelDelta += io.MouseWheel;
			m_hasSceneNavigationInput = true;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_F, false))
		{
			m_sceneNavigationInput.focusRequested = true;
			m_hasSceneNavigationInput = true;
		}
	}

	// Draw Canvas overlay rectangles on top of the image using the ImDrawList API

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// Lamda function to draw a collection of canvas overlay rectangles on the image
	const auto drawCanvasRects = [this, drawList](const std::vector<ViewportCanvasRect>& rects)
		{
			if (rects.empty()) return;

			// Calculate the width and height of the image of the scene view panel
			const float imageWidth = m_imageMax.x - m_imageMin.x;
			const float imageHeight = m_imageMax.y - m_imageMin.y;

			// Push a clipping rectangle to ensure that the
			// overlay rectangles are only drawn within the bounds of the image
			drawList->PushClipRect(m_imageMin, m_imageMax, true);

			// Iterate through each canvas overlay rectangle and draw it on the image
			for (const ViewportCanvasRect& rect : rects)
			{
				ImVec2 canvasCorners[4]{};

				// Calculate the canvas corners in the ImGui window based on the
				// normalized coordinates of the canvas rectangle and the size of the image
				for (size_t i = 0; i < 4; i++)
				{
					canvasCorners[i] =
					{
						m_imageMin.x + rect.corners[i].x * imageWidth,
						m_imageMin.y + rect.corners[i].y * imageHeight
					};
				}

				ImU32 color = IM_COL32(120, 165, 185, 100);
				float thickness = 1.0f;

				// Change the color and thickness of the rectangle based on its role in the canvas overlay
				// Highlight them in the order of EditingRoot > Selected > Ancestor
				switch (rect.role)
				{
				case ViewportCanvasRole::EditingRoot:
					color = IM_COL32(230, 180, 70, 220);
					thickness = 2.0f;
					break;
				case ViewportCanvasRole::Selected:
					color = IM_COL32(90, 190, 255, 255);
					thickness = 2.5f;
					break;
				case ViewportCanvasRole::Ancestor:
					break;
				}

				drawList->AddPolyline(
					canvasCorners, 4, color,
					ImDrawFlags_Closed, thickness
				);
			}

			drawList->PopClipRect();
		};

	if (m_viewMode == EditorViewportMode::Canvas)
	{// Canvas View Mode
		const ImU32 viewportColor = IM_COL32(110, 150, 170, 255);

		// Draw a rectangle around the viewport to indicate the canvas view mode
		drawList->AddRect(m_imageMin, m_imageMax, viewportColor, 0.0f, 0, 2.0f);

		// Draw the canvas overlay rectangles for the currently editing canvas and its ancestors
		drawCanvasRects(overlayData.canvasRects);
	}
	else
	{// Scene View Mode
		// Draw the canvas overlay rectangles for the world-space canvases projected into the scene view
		drawCanvasRects(overlayData.worldCanvasRects);
	}

	// Handle mouse click events for picking actors in the scene
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt)
	{
		// Get the current mouse position in the ImGui window
		const ImVec2 mousePosition = ImGui::GetMousePos();

		// Calculate the width and height of the scene view image
		const float imageWidth = m_imageMax.x - m_imageMin.x;
		const float imageHeight = m_imageMax.y - m_imageMin.y;

		// Calculate the UV coordinates of the mouse click relative to the image
		if (imageWidth > 0.0f &&imageHeight > 0.0f)
		{
			m_pickUV =
			{
				(mousePosition.x - m_imageMin.x) / imageWidth,
				(mousePosition.y - m_imageMin.y) / imageHeight
			};

			m_hasPickRequest = true;
		}
	}

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

bool SceneViewPanel::ConsumePickRequest(Vector2& outViewportUV)
{
	if (!m_hasPickRequest) return false;

	outViewportUV = m_pickUV;
	m_hasPickRequest = false;

	return true;
}

bool SceneViewPanel::ConsumeCanvasOpenRequest(Guid& outCanvasActorGuid)
{
	if (!m_canvasOpenRequest.IsValid()) return false;

	outCanvasActorGuid = m_canvasOpenRequest;
	m_canvasOpenRequest = {};

	return true;
}

bool SceneViewPanel::ConsumeCanvasNavigationInput(CanvasNavigationInput& outInput)
{
	if (!m_hasCanvasNavigationInput) return false;

	outInput = m_canvasNavigationInput;

	m_canvasNavigationInput = {};
	m_hasCanvasNavigationInput = false;

	return true;
}

bool SceneViewPanel::ConsumeSceneNavigationInput(SceneNavigationInput& outInput)
{
	if (!m_hasSceneNavigationInput) return false;

	outInput = m_sceneNavigationInput;
	m_sceneNavigationInput = {};
	m_hasSceneNavigationInput = false;

	return true;
}
