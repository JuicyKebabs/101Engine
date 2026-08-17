#include <cmath>
#include <algorithm>
#include "RendererComponent.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Core/Serialization/JsonMath.h"

std::vector<uint32_t> RendererComponent::BuildCanvasSortPath() const
{
	std::vector<uint32_t> sortPath;

	Canvas* canvas = GetGoverningCanvas();
	if (!canvas) return sortPath; // No governing canvas, return empty path

	// Traverse up the actor hierarchy to collect sort orders from all ancestor canvases
	for (Actor* current = canvas->GetOwner(); current != nullptr; current = current->GetParent())
	{
		Canvas* currentCanvas = current->GetComponentByClass<Canvas>();

		if (currentCanvas)
		{
			sortPath.push_back(currentCanvas->GetSortOrder());
		}
	}

	// Reverse the collected sort orders to have the root canvas first and the current renderer's canvas last
	std::reverse(sortPath.begin(), sortPath.end());

	// Finally, append this renderer's sort order in its governing canvas
	sortPath.push_back(m_sortOrderInCanvas);

	return sortPath;
}

bool RendererComponent::IsVisible() const
{
	if (!m_isVisible) return false;

	// If the governing canvas is set, check its visibility in the hierarchy
	return !m_pGoverningCanvas || m_pGoverningCanvas->IsHierarchyVisible();
}

RenderSpace RendererComponent::GetRenderSpace() const
{
	if (m_pGoverningCanvas) 
	{
		// If the governing canvas is set, determine the render space based on its render mode
		if (m_pGoverningCanvas->GetRenderMode() == CanvasRenderMode::ScreenSpace)
		{// If the governing canvas is in screen space, return Screen render space
			return RenderSpace::Screen;
		}
	}

	// If no governing canvas is set or 
	// if the governing canvas is in world space, 
	// return World render space
	return RenderSpace::World;
}

void RendererComponent::CheckIfTransformChanged()
{
	auto owner = GetOwner();
	if (owner) {
		auto transform = owner->GetComponentByClass<Transform>();
		if (transform) {
			uint64_t currentGeneration = transform->GetWorldGeneration();
			if (m_transformGeneration != currentGeneration) {
				m_transformGeneration = currentGeneration;
				m_isProxyDirty = true;
			}
		}
	}
}

bool RendererComponent::Serialize(
	nlohmann::json& outJson) const
{
	if (!Component::Serialize(outJson)) return false;

	outJson["color"] = JsonMath::ToJson(m_color);
	outJson["visible"] = m_isVisible;
	outJson["sortOrderInCanvas"] = m_sortOrderInCanvas;

	return true;
}

bool RendererComponent::Deserialize(
	const nlohmann::json& json)
{
	if (!json.is_object()) return false;

	// Check for required fields and their types
	if (!json.contains("name")	||
		!json.contains("color") ||
		!json.contains("visible"))
	{
		return false;
	}

	// Validate types of the fields
	if (!json["name"].is_string() ||
		!json["visible"].is_boolean())
	{
		return false;
	}

	std::string parsedName;
	Vector4 parsedColor;
	bool parsedVisible = true;

	// Attempt to read the "name" and "visible" fields
	try
	{
		parsedName = json["name"].get<std::string>();
		parsedVisible = json["visible"].get<bool>();
	}
	catch (const nlohmann::json::exception&)
	{
		return false;
	}

	// Attempt to read the color field using JsonMath::TryRead
	if (!JsonMath::TryRead(json["color"],parsedColor))
	{
		return false;
	}

	// Check if the parsed color values are finite
	if (!std::isfinite(parsedColor.x) ||
		!std::isfinite(parsedColor.y) ||
		!std::isfinite(parsedColor.z) ||
		!std::isfinite(parsedColor.w))
	{
		return false;
	}

	// Optional field: sortOrderInCanvas
	uint32_t parsedSortOrderInCanvas = 0;

	if (json.contains("sortOrderInCanvas"))
	{
		if (!json["sortOrderInCanvas"].is_number_integer()) return false;

		const int64_t parsedOrder = json["sortOrderInCanvas"].get<int64_t>();

		if (parsedOrder < 0 ||
			static_cast<uint64_t>(parsedOrder) >
			static_cast<uint64_t>((std::numeric_limits<uint32_t>::max)()))
		{
			return false;
		}

		parsedSortOrderInCanvas = static_cast<uint32_t>(parsedOrder);
	}

	// Apply only after every field has been validated.
	SetName(parsedName);
	SetColor(parsedColor);
	SetVisible(parsedVisible);
	SetSortOrderInCanvas(parsedSortOrderInCanvas);

	// Reset the transform generation and mark the proxy as dirty
	m_transformGeneration = static_cast<uint64_t>(-1);
	m_isProxyDirty = true;

	return true;
}
