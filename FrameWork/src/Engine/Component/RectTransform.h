#pragma once
#include "Engine/Component/Transform.h"

// Anchor modes enumeration
enum class AnchorMode
{
	TopLeft,
	TopCenter,
	TopRight,
	MiddleLeft,
	MiddleCenter,
	MiddleRight,
	BottomLeft,
	BottomCenter,
	BottomRight
};

class RectTransform : public Transform
{
public:
	struct ParamDesc
	{
		AnchorMode anchorMode = AnchorMode::MiddleCenter;	// Anchor mode (determines how the anchored position is calculated)
		Vector2 anchoredPosition = Vector2::Zero();			// Position relative to the anchor point
		Vector2 pivot = Vector2(0.5f, 0.5f);				// Pivot point (normalized, (0,0) is top-left, (1,1) is bottom-right)
		Vector2 size = Vector2::One();						// Size difference from the original size (used for scaling)
		std::string name = "RectTransform";					// Component name (optional, can be used for debugging or identification)
	};

public:
	RectTransform() = default;
	~RectTransform() = default;
	void SetParams(const ParamDesc& desc) {
		m_anchorMode = desc.anchorMode;
		m_anchoredPosition = desc.anchoredPosition;
		m_pivot = desc.pivot;
		m_size = desc.size;
		SetName(desc.name);
		MarkDirty(); 
	}

	void UpdateGeometry() override;	// Update the geometry of the RectTransform based on its properties (anchored position, pivot, size delta)

	// Setters
	void SetAnchorMode(AnchorMode mode) { m_anchorMode = mode; MarkDirty(); }											// Set the anchor mode and mark as dirty
	void SetAnchoredPosition(const Vector2& anchoredPosition) { m_anchoredPosition = anchoredPosition; MarkDirty(); }	// Set the anchored position and mark as dirty
	void SetPivot(const Vector2& pivot) { m_pivot = pivot; MarkDirty(); }												// Set the pivot and mark as dirty
	void SetSizeDelta(const Vector2& sizeDelta) { m_size = sizeDelta; MarkDirty(); }									// Set the size delta and mark as dirty

	// Getters
	AnchorMode GetAnchorMode() const { return m_anchorMode; }					// Get the anchor mode
	const Vector2& GetAnchoredPosition() const { return m_anchoredPosition; }	// Get the anchored position
	const Vector2& GetPivot() const { return m_pivot; }							// Get the pivot
	const Vector2& GetSize() const { return m_size; }							// Get the size delta

	// Serialization and deserialization methods for
	bool Serialize(nlohmann::json& outJson) const override;
	bool Deserialize(const nlohmann::json& json) override;

private:
	AnchorMode m_anchorMode = AnchorMode::MiddleCenter;	// Anchor mode (determines how the anchored position is calculated)
	Vector2 m_anchoredPosition = Vector2::Zero();		// Position relative to the anchor point
	Vector2 m_pivot = Vector2(0.5f, 0.5f);				// Pivot point (normalized, (0,0) is top-left, (1,1) is bottom-right)
	Vector2 m_size = Vector2::One();					// Size of this ui actor

private:
	// Overrides
	void OnStartOverride() override {};
	void PreUpdateOverride(float deltaTime) override {};
	void UpdateOverride(float deltaTime) override {};
	void LateUpdateOverride(float deltaTime) override {};
	void OnDestroyOverride() override {};

	Vector2 CalcAnchorOffset(AnchorMode anchorMode, const Vector2& parentSize) const;	// Calculate the offset based on the anchor mode and the size of the parent
};