#pragma once
#include <d3d12.h>
#include <vector>
#include <string>
#include "imgui.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Guid/Guid.h"

//--------------------------------------------------------------------------------------------
// SceneViewPanel class
// This class is responsible for rendering the scene view panel in the application.
// Receive GPU handle of the render target and render the scene view panel using ImGui::Imgui.
//--------------------------------------------------------------------------------------------

// Enumration to define the editor viewport mode
enum class EditorViewportMode
{
	Scene,	// Editing the scene in world space (Default)
	Canvas,	// Editing contents in a selected Canvas on the 2D screen space
};

// Enumration of the type of displaying canvas rectangle in the canvas view panel
enum class ViewportCanvasRole
{
	EditingRoot,	// The canvas currently being edited in the canvas view (Most highlighting)
	Ancestor,		// Ancestor canvases of the selected canvas (Subtle highlighting)
	Selected		// Currently selected in the canvas view pannel (Noramal highlighting)
};

// Struct to hold the information for displaying the canvas rectangle overlay in the canvas view panel
struct ViewportCanvasRect
{
	Vector2 corners[4]{};	// The four corners of the canvas rectangle

	// The role of the canvas rectangle overlay, which determines how it is displayed in the canvas view panel
	ViewportCanvasRole role = ViewportCanvasRole::Ancestor;
};

// Enumration of the type of displaying canvas item in the breadcrumb navigation of the canvas view panel
enum class CanvasBreadcrumbItemType
{
	Canvas,		// Represents a canvas item in the breadcrumb navigation
	Ellipsis	// Represents a collapsed ancestor canvas in the breadcrumb navigation
};

// Struct to hold information of Canvas listed in the breadcrumb navigation of the canvas view panel
struct CanvasBreadcrumbItem
{
	CanvasBreadcrumbItemType type = CanvasBreadcrumbItemType::Canvas;
	Guid canvasActorGuid;	// Guid of the Actor for focusing in the canvas view panel
	std::string name;		// Name of the Canvas for displaying in the breadcrumb navigation
};

// Struct to hold the user input for navigating the canvas view panel
// EditorApp will consume this input and update the Canvas View display state accordingly
struct CanvasNavigationInput
{
	Vector2 panDeltaPixels = Vector2::Zero();	// The amount of the pan movement in pixels
	Vector2 zoomPivotUV = { 0.5f, 0.5f };		// The pivot point for zooming in UV coordinates (Following mouse cursor)
	float wheelDelta = 0.0f;					// The amount of mouse wheel input for zooming
	bool fitRequested = false;					// Flag to indicate if the user requested to fit the viewport to the Canvas rectangle
};

// Struct to hold overlay data for the canvas view panel
// This holds the variety of information displayed on the Scene and Canvas view panels
struct ViewportOverlayData
{
	Vector2 referenceSize = Vector2::Zero();	// The reference size of the canvas being edited in the canvas view panel
	UINT canvasOrder = 0;						// The sort order of the canvas being edited in the canvas view panel

	// List of canvas rectangle overlays to be displayed in the canvas view panel
	// Collection of the canvas rectangle of selected Canavs and its ancestor canvases up to currently editing canvas
	std::vector<ViewportCanvasRect> canvasRects;

	// World-Space Canvas rectangles projected into Scene View.
	std::vector<ViewportCanvasRect> worldCanvasRects;

	// List of Canvas items for the breadcrumb navigation in the canvas view panel
	// Ancestors of the current focused Canvas up to the root Canvas in the hierarchy, including the currently focused Canvas
	std::vector<CanvasBreadcrumbItem> canvasBreadcrumbs;
};

class SceneViewPanel
{
public:
	void Render(
		D3D12_GPU_DESCRIPTOR_HANDLE sceneTextureHandle,
		UINT textureWidth, UINT textureHeight,
		const ViewportOverlayData& overlayData
	);

	// Consume a resize request from the scene view panel and return the new width and height
	bool ConsumeResizeRequest(UINT& outWidth, UINT& outHeight);

	// Consume a pick request and return the viewport UV coordinates of the mouse click
	bool ConsumePickRequest(Vector2& outViewportUV);

	// Consume a canvas open request and return the Guid of the canvas to be opened
	bool ConsumeCanvasOpenRequest(Guid& outCanvasActorGuid);

	// Consume a user input for navigating the Canvas View panel
	bool ConsumeCanvasNavigationInput(CanvasNavigationInput& outInput);

	EditorViewportMode GetViewMode() const { return m_viewMode; }
	void SetViewMode(EditorViewportMode mode) { m_viewMode = mode; }

	bool IsHovered() const { return m_isHovered; }
	bool IsFocused() const { return m_isFocused; }

	ImVec2 GetViewportSize() const { return m_viewportSize; }
	ImVec2 GetImageMin() const { return m_imageMin; }
	ImVec2 GetImageMax() const { return m_imageMax; }

private:
	static constexpr float kRenderScale = 1.0f; // Scale factor for rendering the scene view panel

	// Current editor viewport mode (Scene or Canvas)
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

	// Request for picking an Actor in the scene view panel, based on mouse click position
	Vector2 m_pickUV = Vector2::Zero();	// Store the UV coordinates of the mouse click in the viewport for picking actors
	bool m_hasPickRequest = false;		// Flag to indicate if a pick request has been made (mouse click in the viewport)

	// Request for manipulating the breadcrumb navigation in the canvas view panel (clicking on a Canvas item to open it)
	Guid m_canvasOpenRequest;	// Store the Guid of the canvas that will be opened after clicking the breadcrumb item in the canvas view panel

	// Request for reflecting user input for navigating the Canvas View panel (panning, zooming, etc.)
	CanvasNavigationInput m_canvasNavigationInput;		// Store the user input for navigating the Canvas View panel
	bool m_hasCanvasNavigationInput = false;			// Flag to indicate if there is user input for navigating the Canvas View panel
};
