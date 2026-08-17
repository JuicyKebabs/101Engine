#include "Canvas.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Core/Serialization/JsonMath.h"
#include <cmath>
#include <limits>
#include <algorithm>

Vector2 Canvas::GetLayoutReferenceSize() const
{
	Actor* owner = GetOwner();
	if (!owner) return m_referenceSize;

	// World-Space Canvas always uses its authored logical size.
	if (m_effectiveRenderMode == CanvasRenderMode::WorldSpace)
	{
		return m_referenceSize;
	}

	// Scale-With-Screen-Size lays out children in the Canvas's authored
	// logical resolution. GetScaleFactor maps this logical space to the
	// actual display area (viewport for a root Canvas, RectTransform size
	// for a nested Canvas).
	if (m_scaleMode == CanvasScaleMode::ScaleWithScreenSize)
	{
		return m_referenceSize;
	}

	// Constant-Pixel-Size uses the actual display area as its layout space,
	// keeping one layout unit equal to one parent-space pixel unit.
	if (!IsRootCanvas())
	{
		RectTransform* rectTransform =
			owner->GetComponentByClass<RectTransform>();

		return rectTransform
			? rectTransform->GetSize()
			: Vector2::One();
	}

	SceneBase* scene = owner->GetOwner();

	return scene
		? scene->GetViewportSize()
		: Vector2::One();
}

bool Canvas::IsRootCanvas() const
{
	Actor* owner = GetOwner();
	if (!owner) return false;

	// Traverse the hierarchy of parent actors to check if any ancestor has a Canvas component
	for (Actor* ancestor = owner->GetParent(); ancestor; ancestor = ancestor->GetParent())
	{
		if (ancestor->GetComponentByClass<Canvas>())
		{
			return false;
		}
	}

	return true;
}

bool Canvas::IsHierarchyVisible() const
{
	// Traverse the hierarchy of parent actors to check if any ancestor Canvas is not visible
	for (Actor* actor = GetOwner(); actor; actor = actor->GetParent())
	{
		Canvas* canvas = actor->GetComponentByClass<Canvas>();
		if (canvas && !canvas->IsVisible()) return false;
	}

	return true;
}

Matrix4x4 Canvas::GetContentWorldMatrix() const
{
	Actor* owner = GetOwner();
	if (!owner) return Matrix4x4::Identity();

	Transform* transform = owner->GetComponentByClass<Transform>();

	return transform
		? transform->GetWorldTransform().GetMatrix()
		: Matrix4x4::Identity();
}

bool Canvas::ContainsCanvas(const Canvas* canvas) const
{
	if (!canvas) return false;

	Actor* rootActor = GetOwner();
	Actor* current = canvas->GetOwner();

	if (!rootActor || !current) return false;

	for (; current; current = current->GetParent())
	{
		if (current == rootActor) return true;
	}

	return false;
}

bool Canvas::ContainsRenderer(const RendererComponent* renderer) const
{
	return renderer && ContainsCanvas(renderer->GetGoverningCanvas());
}

float Canvas::GetScaleFactor() const
{
	if (m_effectiveRenderMode != CanvasRenderMode::ScreenSpace) return 1.0f;
	if (m_scaleMode == CanvasScaleMode::ConstantPixelSize) return 1.0f;

	Actor* owner = GetOwner();
	if (!owner) return 1.0f;

	Vector2 displaySize = Vector2::One();

	if (IsRootCanvas())
	{
		SceneBase* scene = owner->GetOwner();
		if (!scene) return 1.0f;

		displaySize = scene->GetViewportSize();
	}
	else
	{
		RectTransform* rectTransform =
			owner->GetComponentByClass<RectTransform>();

		if (!rectTransform) return 1.0f;

		displaySize = rectTransform->GetSize();
	}

	if (displaySize.x <= 0.0f ||
		displaySize.y <= 0.0f ||
		m_referenceSize.x <= 0.0f ||
		m_referenceSize.y <= 0.0f)
	{
		return 1.0f;
	}

	// A nested Canvas scales its logical resolution into its RectTransform
	// display size in exactly the same way that a root Canvas scales into
	// the viewport.
	const float scaleX = displaySize.x / m_referenceSize.x;
	const float scaleY = displaySize.y / m_referenceSize.y;

	// Clamp the matchWidthOrHeight value to the range [0, 1]
	const float match = std::clamp(m_matchWidthOrHeight, 0.0f, 1.0f);

	// Calculate the logarithmic scale factor using the match value to
	// interpolate between width and height scales
	const float logWidth = std::log2(scaleX);
	const float logHeight = std::log2(scaleY);
	const float logScale = logWidth + (logHeight - logWidth) * match;

	// Return the final scale factor by exponentiating the logarithmic scale
	return std::pow(2.0f, logScale);
}

bool Canvas::Serialize(
	nlohmann::json& outJson) const
{
	if (!Component::Serialize(outJson)) return false;

	outJson["renderMode"] = static_cast<int>(m_authoredRenderMode);
	outJson["scaleMode"] = static_cast<int>(m_scaleMode);
	outJson["sortOrder"] = m_sortOrder;
	outJson["visible"] = m_isVisible;
	outJson["referenceSize"] = JsonMath::ToJson(m_referenceSize);
	outJson["matchWidthOrHeight"] = m_matchWidthOrHeight;
	
	return true;
}

bool Canvas::Deserialize(
	const nlohmann::json& json)
{
	if (!json.is_object()) return false;

	// Validate the required fields in the JSON object
	if (!json.contains("name")			||
		!json.contains("renderMode")	||
		!json.contains("sortOrder")		||
		!json.contains("visible")		||
		!json.contains("referenceSize")
	)
	{
		return false;
	}

	// Validate the types of the fields
	if (!json["name"].is_string()				||
		!json["renderMode"].is_number_integer()	||
		!json["sortOrder"].is_number_integer()	||
		!json["visible"].is_boolean()			||
		!json["referenceSize"].is_array()
	)
	{
		return false;
	}

	std::string parsedName;
	int64_t parsedRenderMode = 0;
	int64_t parsedScaleMode = static_cast<int64_t>(CanvasScaleMode::ScaleWithScreenSize);
	int64_t parsedSortOrder = 0;
	bool parsedVisible = true;
	Vector2 parsedReferenceSize;
	float parsedMatchWidthOrHeight = 0.5f;

	// Attempt to parse the values from the JSON object
	try
	{
		parsedName = json["name"].get<std::string>();
		parsedRenderMode = json["renderMode"].get<int64_t>();
		parsedSortOrder = json["sortOrder"].get<int64_t>();
		parsedVisible = json["visible"].get<bool>();

		if (json.contains("scaleMode"))
		{
			if (!json["scaleMode"].is_number_integer()) return false;
			parsedScaleMode = json["scaleMode"].get<int64_t>();
		}

		if (json.contains("matchWidthOrHeight"))
		{
			if (!json["matchWidthOrHeight"].is_number()) return false;
			parsedMatchWidthOrHeight = json["matchWidthOrHeight"].get<float>();
		}
		if (!JsonMath::TryRead(json["referenceSize"], parsedReferenceSize))
		{
			return false;
		}
	}
	catch (const nlohmann::json::exception&)
	{
		return false;
	}

	// Validate the parsed render mode to ensure it is within the valid range of the CanvasRenderMode enum
	if (parsedRenderMode < static_cast<int64_t>(CanvasRenderMode::ScreenSpace) ||
		parsedRenderMode >= static_cast<int64_t>(CanvasRenderMode::Max))
	{
		return false;
	}

	// Validate the parsed scale mode to ensure it is within the valid range of the CanvasScaleMode enum
	if (parsedScaleMode < static_cast<int64_t>(CanvasScaleMode::ConstantPixelSize) ||
		parsedScaleMode >= static_cast<int64_t>(CanvasScaleMode::Max))
	{
		return false;
	}

	// Validate the parsed sort order to ensure it is within the valid range for UINT
	if (parsedSortOrder < 0 ||
		static_cast<uint64_t>(parsedSortOrder) >
		static_cast<uint64_t>((std::numeric_limits<UINT>::max)()))
	{
		return false;
	}

	// Validate the parsed reference size to ensure it is finite and positive
	if (!std::isfinite(parsedReferenceSize.x) ||
		!std::isfinite(parsedReferenceSize.y) ||
		parsedReferenceSize.x <= 0.0f || parsedReferenceSize.y <= 0.0f)
	{
		return false;
	}

	// Validate the parsed match width or height to ensure it is finite and within the range [0.0, 1.0]
	if (!std::isfinite(parsedMatchWidthOrHeight) ||
		parsedMatchWidthOrHeight < 0.0f 
		|| parsedMatchWidthOrHeight > 1.0f)
	{
		return false;
	}

	// Apply the parsed values to the Canvas component's parameters
	ParamDesc parsedDesc;
	parsedDesc.name = parsedName;
	parsedDesc.renderMode = static_cast<CanvasRenderMode>(parsedRenderMode);
	parsedDesc.scaleMode = static_cast<CanvasScaleMode>(parsedScaleMode);
	parsedDesc.sortOrder = static_cast<UINT>(parsedSortOrder);
	parsedDesc.isVisible = parsedVisible;
	parsedDesc.referenceSize = parsedReferenceSize;
	parsedDesc.matchWidthOrHeight = parsedMatchWidthOrHeight;

	SetParams(parsedDesc);

	return true;
}
