#include <cmath>
#include "RendererComponent.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/Serialization/JsonMath.h"

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

	// Apply the parsed values to the component
	SetName(parsedName);
	SetColor(parsedColor);
	SetVisible(parsedVisible);

	// Reset the transform generation and mark the proxy as dirty
	m_transformGeneration = static_cast<uint64_t>(-1);
	m_isProxyDirty = true;

	return true;
}