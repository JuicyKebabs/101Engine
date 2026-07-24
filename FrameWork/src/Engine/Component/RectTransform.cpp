#include <cmath>
#include "RectTransform.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Window/WindowInfo.h"
#include "Engine/Core/Serialization/JsonMath.h"

void RectTransform::UpdateGeometry()
{
	if (!m_isDirty) return;

	// Get transform component of the parent actor
	auto owner = GetOwner();
	auto parent = owner ? owner->GetParent() : nullptr;
	auto parentRT = parent ? parent->GetComponentByClass<RectTransform>() : nullptr;

	// Calculate parent size (if parent has RectTransform, use its size; otherwise, use window size)
	Vector2 parentSize = parentRT ? parentRT->GetSize() 
		: Vector2(static_cast<float>(WindowInfo::GetInstance().GetWidth()), static_cast<float>(WindowInfo::GetInstance().GetHeight()));

	// Calculate the world position by adding the anchor offset and anchored position to the parent's world position (if parent exists)
	Vector3 parentWorldPos = parentRT ? parentRT->GetWorldPosition() : Vector3::Zero();

	// Calculate anchor offset based on the anchor mode and parent size
	Vector2 anchorOffset = CalcAnchorOffset(m_anchorMode, parentSize);

	// Calculate pivot offset based on the pivot point and size of the UI element
	Vector2 pivotOffset(
		(0.5f - m_pivot.x) * m_size.x,	// Pivot offset in X direction (centered at 0.5)
		(0.5f - m_pivot.y) * m_size.y	// Pivot offset in Y direction (centered at 0.5)
	);
	
	// Calculate the final UI position
	Vector3 uiPosition(
		parentWorldPos.x + anchorOffset.x + m_anchoredPosition.x + pivotOffset.x,
		parentWorldPos.y + anchorOffset.y + m_anchoredPosition.y + pivotOffset.y,
		m_localTransform.position.z
	);

	// The scale is determined by the size of the UI element (width and height)
	Vector3 uiScale(
		m_size.x,
		m_size.y,
		1.0f
	);

	// Calculate the world matrix (Isolated from parent matrix)
	m_worldMatrix = Matrix4x4::CreateTRS(uiPosition, m_localTransform.rotation, uiScale);

	// Save world transform by decomposing the world matrix
	m_worldMatrix.Decompose(
		m_worldTransform.position, 
		m_worldTransform.rotation, 
		m_worldTransform.scale
	);

	m_worldGeneration++;
	m_isDirty = false;
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

	// Assign the parsed values to the member variables
	m_anchorMode = parsedDesc.anchorMode;
	m_anchoredPosition = parsedDesc.anchoredPosition;
	m_pivot = parsedDesc.pivot;
	m_size = parsedDesc.size;

	// Mark the RectTransform as dirty to indicate that its geometry needs to be updated
	MarkDirty();

	return true;
}