#include <cmath>
#include "RectTransform.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Core/Serialization/JsonMath.h"

void RectTransform::UpdateGeometry()
{
	if (!m_isDirty) return;

	// Get the parent Transform and its RectTransform specialization, if available
	Actor* owner = GetOwner();
	Actor* parent = owner ? owner->GetParent() : nullptr;
	Transform* parentTransform = parent ? parent->GetComponentByClass<Transform>() : nullptr;
	RectTransform* parentRectTransform = parent ? parent->GetComponentByClass<RectTransform>() : nullptr;

	// Resolve the reference size used to build the layout transform.
	const Vector2 referenceSize = ResolveLayoutReferenceSize(
		owner,
		parent,
		parentRectTransform
	);

	// Build the layout transform based on the reference size and properties of this RectTransform.
	Transform3D layoutTransform = BuildLayoutTransform(referenceSize);

	// Resolve the final hierarchy transform.
	// Only the topmost Screen-Space Canvas ignores the ancestor's 3D hierarchy.
	Transform3D resolvedWorldTransform;

	if (parentTransform && !IsRootScreenSpaceCanvas(owner))
	{// This RectTransform has a parent and is not the topmost Screen-Space Canvas

		// Inherit the world transform from the parent Transform 
		// and combine it with the layout transform of this RectTransform.
		resolvedWorldTransform = CombineTransform3D(
			parentTransform->GetWorldTransform(),
			layoutTransform
		);
	}
	else
	{
		// The topmost Screen-Space Canvas establishes an independent
		// viewport-based coordinate space and ignores its 3D parent.
		// An unparented RectTransform also has nothing to inherit.
		resolvedWorldTransform = layoutTransform;
	}

	// Update the local matrix based on the layout transform
	m_localMatrix = layoutTransform.GetMatrix();

	// Apply the RectTransform size only to the render matrix.
	// The size is excluded from m_worldTransform so that it does not
	// multiply the position or scale of child RectTransforms.
	const Vector3 renderScale(
		resolvedWorldTransform.scale.x * m_size.x,
		resolvedWorldTransform.scale.y * m_size.y,
		resolvedWorldTransform.scale.z
	);

	// Build this RectTransform's own visible rectangle before applying its
	// Canvas scale. A nested Canvas therefore keeps the pixel size specified
	// by its RectTransform instead of shrinking its own frame.
	m_worldMatrix = Matrix4x4::CreateTRS(
		resolvedWorldTransform.position,
		resolvedWorldTransform.rotation,
		renderScale
	);

	// Children inherit the resolved pose. If this Actor owns a Screen-Space
	// Canvas, also include the factor that maps the Canvas's logical reference
	// resolution into its display area. Keeping this factor out of m_worldMatrix
	// separates the Canvas frame from the coordinate system inherited by children.
	m_worldTransform = resolvedWorldTransform;

	Canvas* ownerCanvas = owner
		? owner->GetComponentByClass<Canvas>()
		: nullptr;

	if (ownerCanvas &&
		ownerCanvas->GetRenderMode() == CanvasRenderMode::ScreenSpace)
	{
		const float canvasScale = ownerCanvas->GetScaleFactor();

		m_worldTransform.scale.x *= canvasScale;
		m_worldTransform.scale.y *= canvasScale;
		m_worldTransform.scale.z *= canvasScale;
	}

	m_worldGeneration++;
	m_isDirty = false;
}

Vector2 RectTransform::ResolveLayoutReferenceSize(
	Actor* owner,
	Actor* parent,
	RectTransform* parentRectTransform
) const
{
	Canvas* parentCanvas = parent
		? parent->GetComponentByClass<Canvas>()
		: nullptr;

	// If parent has a Canvas, use its layout reference size
	if (parentCanvas) return parentCanvas->GetLayoutReferenceSize();

	// If parent has a RectTransform with no Canvas, use its size
	if (parentRectTransform) return parentRectTransform->GetSize();

	Canvas* ownerCanvas = owner
		? owner->GetComponentByClass<Canvas>()
		: nullptr;

	// If the parent has neither a Canvas nor a RectTransform,
	// use the owner's Canvas layout reference size when this
	// RectTransform belongs to a Canvas Actor.
	if (ownerCanvas) return ownerCanvas->GetLayoutReferenceSize();

	// If no Canvas is found, use the viewport size of the scene as a fallback
	SceneBase* scene = owner ? owner->GetOwner() : nullptr;
	return scene ? scene->GetViewportSize() : Vector2::One();
}

Transform3D RectTransform::BuildLayoutTransform(const Vector2& referenceSize) const
{
	// Calculate the anchor offset based on the anchor mode and reference size
	const Vector2 anchorOffset = CalcAnchorOffset(m_anchorMode, referenceSize);
	const Vector2 pivotOffset(
		(0.5f - m_pivot.x) * m_size.x,
		(0.5f - m_pivot.y) * m_size.y
	);

	// Apply anchor offset, anchored position, and pivot offset 
	// to the local transform to get the final layout transform
	Transform3D layoutTransform = m_localTransform;
	layoutTransform.position = Vector3(
		anchorOffset.x + m_anchoredPosition.x + pivotOffset.x,
		anchorOffset.y + m_anchoredPosition.y + pivotOffset.y,
		0.0f	// Ignore Z position to keep the RectTransform in 2D space on Canvas
	);

	return layoutTransform;
}

bool RectTransform::IsRootScreenSpaceCanvas(Actor* owner) const
{
	// Try to get the canvas component from the owner actor
	Canvas* canvas = owner
		? owner->GetComponentByClass<Canvas>()
		: nullptr;

	// If the owner does not have a canvas or the canvas is not in screen-space mode, return false
	if (!canvas || canvas->GetRenderMode() != CanvasRenderMode::ScreenSpace)
	{
		return false;
	}

	// Check if any ancestor actor has a canvas component
	for (Actor* ancestor = owner->GetParent(); ancestor; ancestor = ancestor->GetParent())
	{
		// If any ancestor has a canvas, then this is not a root screen-space canvas
		if (ancestor->GetComponentByClass<Canvas>()) return false;
	}

	return true;
}

Vector2 RectTransform::CalcAnchorOffset(AnchorMode mode, const Vector2& parentSize) const
{
	const float halfWidth = parentSize.x * 0.5f;
	const float halfHeight = parentSize.y * 0.5f;

	switch (mode)
	{
	case AnchorMode::TopLeft:		return Vector2(-halfWidth, halfHeight);
	case AnchorMode::TopCenter:		return Vector2(0, halfHeight);
	case AnchorMode::TopRight:		return Vector2(halfWidth, halfHeight);
	case AnchorMode::MiddleLeft:	return Vector2(-halfWidth, 0.0f);
	case AnchorMode::MiddleCenter:	return Vector2(0.0f, 0.0f);
	case AnchorMode::MiddleRight:	return Vector2(halfWidth, 0.0f);
	case AnchorMode::BottomLeft:	return Vector2(-halfWidth, -halfHeight);
	case AnchorMode::BottomCenter:	return Vector2(0.0f, -halfHeight);
	case AnchorMode::BottomRight:	return Vector2(halfWidth, -halfHeight);
	default:						return Vector2(0.0f, 0.0f);	
	}
}

bool RectTransform::Serialize(nlohmann::json& outJson) const
{
	if (!Transform::Serialize(outJson)) return false;

	outJson["anchorMode"] = static_cast<int>(m_anchorMode);
	outJson["anchoredPosition"] = JsonMath::ToJson(m_anchoredPosition);
	outJson["pivot"] = JsonMath::ToJson(m_pivot);
	outJson["size"] = JsonMath::ToJson(m_size);

	return true;
}

bool RectTransform::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object()) return false;

	// Check for required fields in the JSON object
	if (!json.contains("anchorMode") ||
		!json.contains("anchoredPosition") ||
		!json.contains("pivot") ||
		!json.contains("size"))
	{
		return false;
	}

	// Validate the types of the fields
	if (!json["anchorMode"].is_number_integer()) return false;

	int parsedAnchorMode = 0;

	ParamDesc parsedDesc;

	// Read anchorMode from JSON and handle exceptions
	try
	{
		parsedAnchorMode = json["anchorMode"].get<int>();
	}
	catch (const nlohmann::json::exception&)
	{
		return false;
	}
	
	// Try to read the other fields from JSON into the parsedDesc structure
	if (!JsonMath::TryRead(json["anchoredPosition"], parsedDesc.anchoredPosition)	||
		!JsonMath::TryRead(json["pivot"], parsedDesc.pivot)							||
		!JsonMath::TryRead(json["size"], parsedDesc.size))
	{
		return false;
	}

	// Validate the parsed anchor mode
	constexpr int kAnchorModeMin = static_cast<int>(AnchorMode::TopLeft);
	constexpr int kAnchorModeMax = static_cast<int>(AnchorMode::BottomRight);

	if (parsedAnchorMode < kAnchorModeMin || parsedAnchorMode > kAnchorModeMax)
	{
		return false;
	}

	// Check if the parsed Vector2 values are finite (not NaN or infinity)
	const auto isFiniteVector2 =
		[](const Vector2& value)
		{
			return std::isfinite(value.x) && std::isfinite(value.y);
		};

	if (!isFiniteVector2(parsedDesc.anchoredPosition)	||
		!isFiniteVector2(parsedDesc.pivot)				||
		!isFiniteVector2(parsedDesc.size))
	{
		return false;
	}

	// Validate that the pivot values are within the range [0.0, 1.0]
	if (parsedDesc.pivot.x < 0.0f ||
		parsedDesc.pivot.x > 1.0f ||
		parsedDesc.pivot.y < 0.0f ||
		parsedDesc.pivot.y > 1.0f)
	{
		return false;
	}

	// Validate that the size values are non-negative
	if (parsedDesc.size.x < 0.0f ||
		parsedDesc.size.y < 0.0f)
	{
		return false;
	}

	// If all validations pass, assign the parsed values to the member variables
	parsedDesc.anchorMode = static_cast<AnchorMode>(parsedAnchorMode);

	// Set the parameters of the RectTransform using the parsed values
	if (!Transform::Deserialize(json)) return false;
	m_localTransform.position = Vector3::Zero();

	// Assign the parsed values to the member variables
	m_anchorMode = parsedDesc.anchorMode;
	m_anchoredPosition = parsedDesc.anchoredPosition;
	m_pivot = parsedDesc.pivot;
	m_size = parsedDesc.size;

	// Mark the RectTransform as dirty to indicate that its geometry needs to be updated
	MarkDirty();

	return true;
}
