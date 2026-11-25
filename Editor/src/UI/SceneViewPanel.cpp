#include <algorithm>
#include <cmath>
#include "SceneViewPanel.h"
#include "imgui.h"
#include "ImGuizmo.h"

void SceneViewPanel::Render(
	ImTextureID sceneTextureHandle,
	UINT textureWidth,
	UINT textureHeight,
	EditorViewportContext* viewportContext,
	const ViewportOverlayData& overlayData,
	bool canEditScene,
	SceneTransformGizmo* transformGizmo,
	CanvasRectGizmo* canvasGizmo,
	std::span<const EditorDocumentInfo> documents,
	const DocumentTabBar::Callbacks& documentCallbacks
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
	m_documentTabBar.Render(documents, documentCallbacks);

	if (!viewportContext)
	{
		ImGui::TextDisabled("No document is open.");
		ImGui::End();
		return;
	}

	const auto getViewMode = [viewportContext]() { return viewportContext->GetViewMode(); };
	const bool isDraggingCanvasRect = canvasGizmo &&
		canvasGizmo->activeHandle != CanvasRectHandle::None;

	// Display the resolution of the render target
	ImGui::Text(
		"Scene View Resolution: %u x %u",
		textureWidth,
		textureHeight
	);

	// Display the reference size and canvas order if in Canvas view mode and a canvas is present
	if (getViewMode() == EditorViewportMode::Canvas && !overlayData.canvasRects.empty())
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
	if (getViewMode() == EditorViewportMode::Canvas && !overlayData.canvasBreadcrumbs.empty())
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
	if (getViewMode() == EditorViewportMode::Canvas)
	{
		ImGui::SameLine();

		ImGui::BeginDisabled(isDraggingCanvasRect);
		if (ImGui::SmallButton("Fit"))
		{
			m_canvasNavigationInput.fitRequested = true;
			m_hasCanvasNavigationInput = true;
		}
		ImGui::EndDisabled();
	}

	// Render the tab bar for switching between Scene and Canvas view modes
	if (ImGui::BeginTabBar("##EditorViewportTabs"))
	{
		if (ImGui::BeginTabItem("Scene"))
		{
			viewportContext->SetViewMode(EditorViewportMode::Scene);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Canvas"))
		{
			viewportContext->SetViewMode(EditorViewportMode::Canvas);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	const ImGuiIO& io = ImGui::GetIO();
	const bool isCameraGesture = io.KeyAlt ||
		ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
		ImGui::IsMouseDown(ImGuiMouseButton_Middle);
	const bool isDraggingTransform = transformGizmo && transformGizmo->isDragging;
	const bool acceptsToolShortcut = m_isFocused && !io.WantTextInput &&
		!io.KeyCtrl && !io.KeyShift && !io.KeySuper;

	if (canEditScene && getViewMode() == EditorViewportMode::Scene)
	{
		DrawTransformTools(!isCameraGesture && !isDraggingTransform, acceptsToolShortcut);
	}
	else if (canEditScene && getViewMode() == EditorViewportMode::Canvas)
	{
		DrawCanvasRectTools(!isCameraGesture && !isDraggingCanvasRect, acceptsToolShortcut);
	}

	// Get the available size of the content region in the ImGui window
	const ImVec2 availableSize = ImGui::GetContentRegionAvail();

	if (availableSize.x <= 0.0f ||
		availableSize.y <= 0.0f ||
		sceneTextureHandle == ImTextureID_Invalid ||
		textureWidth == 0 ||
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

	// Aspect ratio calculations to maintain the original texture's aspect ratio while fitting within the available size
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
		sceneTextureHandle,
		{ imageWidth, imageHeight },
		{ 0.0f, 0.0f },
		{ 1.0f, 1.0f });

	// Store the minimum and maximum coordinates of the image in the ImGui window for later use
	m_imageMin = ImGui::GetItemRectMin();
	m_imageMax = ImGui::GetItemRectMax();

	const bool imageHovered = ImGui::IsItemHovered();
	const bool imageClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
	const bool cameraInput = imageHovered &&
		(isCameraGesture || io.MouseWheel != 0.0f || ImGui::IsKeyPressed(ImGuiKey_F, false));
	bool blocksPicking = isDraggingTransform || isDraggingCanvasRect;

	if (transformGizmo && getViewMode() == EditorViewportMode::Scene)
	{
		if (isDraggingTransform || !cameraInput)
		{
			blocksPicking = DrawTransformGizmo(*transformGizmo, imageHovered);
		}
	}

	// Mouse input detection in Canvas View
	if (getViewMode() == EditorViewportMode::Canvas && imageHovered && !isDraggingCanvasRect)
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
	else if (getViewMode() == EditorViewportMode::Scene && imageHovered && !isDraggingTransform)
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
		if (rects.empty())
		{
			return;
		}

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
						m_imageMin.y + rect.corners[i].y * imageHeight};
			}

			ImU32 color = IM_COL32(
				120,
				165,
				185,
				100);
			float thickness = 1.0f;

			// Change the color and thickness of the rectangle based on its role in the canvas overlay
			// Highlight them in the order of EditingRoot > Selected > Ancestor
			switch (rect.role)
			{
			case ViewportCanvasRole::EditingRoot:
				color = IM_COL32(
					230,
					180,
					70,
					220);
				thickness = 2.0f;
				break;
			case ViewportCanvasRole::Selected:
				color = IM_COL32(
					90,
					190,
					255,
					255);
				thickness = 2.5f;
				break;
			case ViewportCanvasRole::Ancestor:
				break;
			}

			drawList->AddPolyline(
				canvasCorners,
				4,
				color,
				ImDrawFlags_Closed,
				thickness);
		}

		drawList->PopClipRect();
	};

	if (getViewMode() == EditorViewportMode::Canvas)
	{// Canvas View Mode
		const ImU32 viewportColor = IM_COL32(
			110,
			150,
			170,
			255);

		// Draw a rectangle around the viewport to indicate the canvas view mode
		drawList->AddRect(
			m_imageMin,
			m_imageMax,
			viewportColor,
			0.0f,
			0,
			2.0f);

		// Draw the canvas overlay rectangles for the currently editing canvas and its ancestors
		drawCanvasRects(overlayData.canvasRects);

		if (canvasGizmo && m_canvasRectTool != CanvasRectTool::View)
		{
			const bool isNavigating = isCameraGesture || io.MouseWheel != 0.0f ||
				m_canvasNavigationInput.fitRequested;
			const bool canStart = imageClicked && !isNavigating && !ImGui::IsAnyItemActive();
			blocksPicking |= DrawCanvasRectHandles(*canvasGizmo, canStart);
		}
	}
	else
	{// Scene View Mode
		// Draw the canvas overlay rectangles for the world-space canvases projected into the scene view
		drawCanvasRects(overlayData.worldCanvasRects);
	}

	// Handle mouse click events for picking actors in the scene
	if (imageClicked && !io.KeyAlt && !blocksPicking)
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

void SceneViewPanel::DrawCanvasRectTools(bool canSwitch, bool acceptsKeyboard)
{
	ImGui::BeginDisabled(!canSwitch);
	const auto drawTool = [&](const char* label, ImGuiKey key, CanvasRectTool tool)
	{
		const bool clicked = ImGui::RadioButton(label, m_canvasRectTool == tool);
		const bool shortcut = acceptsKeyboard && ImGui::IsKeyPressed(key, false);
		if (canSwitch && (clicked || shortcut))
		{
			m_canvasRectTool = tool;
		}
	};

	drawTool("View (Q)", ImGuiKey_Q, CanvasRectTool::View);
	ImGui::SameLine();
	drawTool("Rect (W)", ImGuiKey_W, CanvasRectTool::Rect);
	ImGui::SameLine();
	drawTool("Pivot (E)", ImGuiKey_E, CanvasRectTool::Pivot);

	ImGui::EndDisabled();
}

bool SceneViewPanel::DrawCanvasRectHandles(CanvasRectGizmo& gizmo, bool canStart)
{
	const float width = m_imageMax.x - m_imageMin.x;
	const float height = m_imageMax.y - m_imageMin.y;
	const ImVec2 mouse = ImGui::GetMousePos();

	gizmo.wasDrawn = true;
	if (ImGui::IsMousePosValid(&mouse))
	{
		gizmo.mouseUV = Vector2((mouse.x - m_imageMin.x) / width, (mouse.y - m_imageMin.y) / height);
	}

	const auto& corners = gizmo.outline.corners;
	const Vector2 center = (corners[0] + corners[2]) * 0.5f;
	const Vector2 pivot = corners[0] + (corners[1] - corners[0]) * gizmo.pivot.x +
		(corners[3] - corners[0]) * gizmo.pivot.y;
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->PushClipRect(m_imageMin, m_imageMax, true);

	const auto drawHandle = [&](CanvasRectHandle handle, const Vector2& uv, bool enabled)
	{
		constexpr float radius = 6.0f;
		const ImVec2 point(m_imageMin.x + uv.x * width, m_imageMin.y + uv.y * height);
		const bool hit = std::fabs(mouse.x - point.x) <= radius &&
			std::fabs(mouse.y - point.y) <= radius;
		const bool canPress = canStart && enabled && hit;
		const bool isIdle = gizmo.activeHandle == CanvasRectHandle::None &&
			gizmo.pressedHandle == CanvasRectHandle::None;

		if (canPress && isIdle)
		{
			gizmo.pressedHandle = handle;
		}

		ImU32 color = enabled ? IM_COL32(75, 183, 255, 255) : IM_COL32(125, 135, 145, 180);
		if (gizmo.activeHandle == handle || gizmo.pressedHandle == handle)
		{
			color = IM_COL32(255, 191, 72, 255);
		}

		if (handle == CanvasRectHandle::Resize)
		{
			const ImVec2 min(point.x - radius, point.y - radius);
			const ImVec2 max(point.x + radius, point.y + radius);
			drawList->AddRectFilled(min, max, color);
		}
		else
		{
			drawList->AddCircleFilled(point, radius, color);
		}
	};

	if (m_canvasRectTool == CanvasRectTool::Pivot)
	{
		drawHandle(CanvasRectHandle::Pivot, pivot, gizmo.canDragPivot);
	}
	else
	{
		// The resize handle wins if a small rectangle makes the hit areas overlap.
		drawHandle(CanvasRectHandle::Resize, corners[2], gizmo.canResize);
		drawHandle(CanvasRectHandle::Move, center, gizmo.canMove);
	}

	drawList->PopClipRect();
	return gizmo.activeHandle != CanvasRectHandle::None || gizmo.pressedHandle != CanvasRectHandle::None;
}

void SceneViewPanel::DrawTransformTools(bool canSwitch, bool acceptsKeyboard)
{
	// Show the buttons to switch betweenthe different transform tools
	// (View, Move, Rotate, Scale)
	struct ToolButton
	{
		const char* label;
		ImGuiKey key;
		SceneTransformTool tool;
	};

	static constexpr ToolButton tools[] = {
		{ "View (Q)", ImGuiKey_Q, SceneTransformTool::View },
		{ "Move (W)", ImGuiKey_W, SceneTransformTool::Move },
		{ "Rotate (E)", ImGuiKey_E, SceneTransformTool::Rotate },
		{ "Scale (R)", ImGuiKey_R, SceneTransformTool::Scale }
	};

	ImGui::BeginDisabled(!canSwitch);

	for (const ToolButton& button : tools)
	{
		const bool clicked = ImGui::RadioButton(button.label, m_transformTool == button.tool);
		const bool shortcut = acceptsKeyboard && ImGui::IsKeyPressed(button.key, false);

		if (canSwitch && (clicked || shortcut))
		{
			m_transformTool = button.tool;
		}

		ImGui::SameLine();
	}

	const bool canSwitchAxes =
		m_transformTool == SceneTransformTool::Move ||
		m_transformTool == SceneTransformTool::Rotate;
	const bool localAxes = m_useLocalTransformAxes || m_transformTool == SceneTransformTool::Scale;
	
	ImGui::BeginDisabled(!canSwitchAxes);

	if (ImGui::SmallButton(localAxes ? "Local" : "Global"))
	{
		m_useLocalTransformAxes = !m_useLocalTransformAxes;
	}

	ImGui::EndDisabled();
	ImGui::EndDisabled();
}

bool SceneViewPanel::DrawTransformGizmo(SceneTransformGizmo& gizmo, bool imageHovered)
{
	gizmo.tool = m_transformTool;

	if (gizmo.tool == SceneTransformTool::View)
	{
		return false;
	}

	const Vector3 viewPosition = gizmo.view.TransformPoint(gizmo.matrix.GetTranslation());
	const float clipDepth = viewPosition.z * gizmo.projection._33 + gizmo.projection._43;
	constexpr float minimumClipDepth = 0.001f;

	// ImGuizmo's perspective early return leaves its draw-list clip stack unbalanced.
	if (!std::isfinite(clipDepth) || clipDepth <= minimumClipDepth)
	{
		return gizmo.isDragging;
	}

	ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;

	if (gizmo.tool == SceneTransformTool::Rotate)
	{
		operation = ImGuizmo::ROTATE;
	}
	else if (gizmo.tool == SceneTransformTool::Scale)
	{
		operation = ImGuizmo::SCALE;
	}

	const bool canUseTool = gizmo.tool != SceneTransformTool::Move || gizmo.canMove;
	const bool canStart = imageHovered && canUseTool;
	const bool localAxes = m_useLocalTransformAxes || operation == ImGuizmo::SCALE;
	const ImGuizmo::MODE mode = localAxes ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
	ImGuizmo::SetRect(
		m_imageMin.x, m_imageMin.y,
		m_imageMax.x - m_imageMin.x,
		m_imageMax.y - m_imageMin.y);
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::Enable(canStart || gizmo.isDragging);

	gizmo.wasDrawn = true;
	gizmo.changed =
		ImGuizmo::Manipulate(
			&gizmo.view._11,
			&gizmo.projection._11,
			operation, mode,
			&gizmo.matrix._11);
	gizmo.isUsing = ImGuizmo::IsUsing();

	return gizmo.isDragging || gizmo.isUsing || (canStart && ImGuizmo::IsOver(operation));
}

void SceneViewPanel::ResetTransformGizmo()
{
	ImGuizmo::Enable(false);
}

bool SceneViewPanel::ConsumeResizeRequest(UINT& outWidth, UINT& outHeight)
{
	if (!m_isViewportResized)
	{
		return false;
	}

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
	if (!m_hasPickRequest)
	{
		return false;
	}

	outViewportUV = m_pickUV;
	m_hasPickRequest = false;

	return true;
}

bool SceneViewPanel::ConsumeCanvasOpenRequest(Guid& outCanvasActorGuid)
{
	if (!m_canvasOpenRequest.IsValid())
	{
		return false;
	}

	outCanvasActorGuid = m_canvasOpenRequest;
	m_canvasOpenRequest = {};

	return true;
}

bool SceneViewPanel::ConsumeCanvasNavigationInput(CanvasNavigationInput& outInput)
{
	if (!m_hasCanvasNavigationInput)
	{
		return false;
	}

	outInput = m_canvasNavigationInput;

	m_canvasNavigationInput = {};
	m_hasCanvasNavigationInput = false;

	return true;
}

bool SceneViewPanel::ConsumeSceneNavigationInput(SceneNavigationInput& outInput)
{
	if (!m_hasSceneNavigationInput)
	{
		return false;
	}

	outInput = m_sceneNavigationInput;
	m_sceneNavigationInput = {};
	m_hasSceneNavigationInput = false;

	return true;
}
