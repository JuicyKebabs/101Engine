#include "Canvas.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Core/Serialization/JsonMath.h"
#include <cmath>
#include <limits>

Vector2 Canvas::GetLayoutReferenceSize() const
{
	Actor* owner = GetOwner();

	if (!owner) return m_worldReferenceSize;

	bool hasCanvasAncestor = false;	// Flag to indicate if there is the canvas ancestor in the hierarchy

	// Traverse the actor hierarchy to check for a Canvas ancestor
	for (Actor* ancestor = owner->GetParent(); ancestor; ancestor = ancestor->GetParent())
	{
		if (ancestor->GetComponentByClass<Canvas>())
		{
			hasCanvasAncestor = true;
			break;
		}
	}

	if (hasCanvasAncestor)
	{// In case of having a canvas ancestor
		// Use owner's RectTransform size (An actor under a canvas has a RectTransform)
		RectTransform* rectTransform = owner->GetComponentByClass<RectTransform>();
		return rectTransform ? rectTransform->GetSize() : Vector2::One();
	}

	if (m_renderMode == CanvasRenderMode::ScreenSpace)
	{// In case of Root-Screen-Space Canvas (no canvas ancestor and render mode is ScreenSpace)
		// Return viewport size registerd in belonging scene
		SceneBase* scene = owner->GetOwner();
		return scene ? scene->GetViewportSize() : Vector2::One();
	}

	// In case of Root-World-Space Canvas (no canvas ancestor and render mode is WorldSpace)
	// Return the logical reference size for layout calculations
	return m_worldReferenceSize;
}

bool Canvas::Serialize(
	nlohmann::json& outJson) const
{
	if (!Component::Serialize(outJson)) return false;

	outJson["renderMode"] = static_cast<int>(m_renderMode);
	outJson["sortOrder"] = m_sortOrder;
	outJson["visible"] = m_isVisible;
	outJson["referenceSize"] = JsonMath::ToJson(m_worldReferenceSize);

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
		!json.contains("referenceSize"))
	{
		return false;
	}

	// Validate the types of the fields
	if (!json["name"].is_string()				||
		!json["renderMode"].is_number_integer()	||
		!json["sortOrder"].is_number_integer()	||
		!json["visible"].is_boolean()			||
		!json["referenceSize"].is_array())
	{
		return false;
	}

	std::string parsedName;
	int64_t parsedRenderMode = 0;
	int64_t parsedSortOrder = 0;
	bool parsedVisible = true;
	Vector2 parsedReferenceSize;

	// Attempt to parse the values from the JSON object
	try
	{
		parsedName = json["name"].get<std::string>();
		parsedRenderMode = json["renderMode"].get<int64_t>();
		parsedSortOrder = json["sortOrder"].get<int64_t>();
		parsedVisible = json["visible"].get<bool>();
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

	// Apply the parsed values to the Canvas component's parameters
	ParamDesc parsedDesc;
	parsedDesc.name = parsedName;
	parsedDesc.renderMode = static_cast<CanvasRenderMode>(parsedRenderMode);
	parsedDesc.sortOrder = static_cast<UINT>(parsedSortOrder);
	parsedDesc.isVisible = parsedVisible;
	parsedDesc.referenceSize = parsedReferenceSize;

	SetParams(parsedDesc);

	return true;
}
