#pragma once
#include "Engine/Component/Transform.h"

class RectTransform : public Transform
{
public:
	struct InitDesc : public Transform::InitDesc
	{
		Vector2 anchoredPosition;	// Position relative to the anchor point
		Vector2 pivot;				// Pivot point (normalized, (0,0) is top-left, (1,1) is bottom-right)
		Vector2 sizeDelta;			// Size difference from the original size (used for scaling)
		InitDesc(Vector3 localPosition = Vector3::Zero(), Vector3 localEulerDeg = Vector3::Zero(), Vector3 localScale = Vector3::One(),
			Vector2 anchoredPosition = Vector2::Zero(), Vector2 pivot = Vector2(0.5f, 0.5f), Vector2 sizeDelta = Vector2::One(),
			const std::string& name = "RectTransform")
			: Transform::InitDesc(localPosition, localEulerDeg, localScale, name),
			  anchoredPosition(anchoredPosition), pivot(pivot), sizeDelta(sizeDelta) {}
	};

public:
	RectTransform() = default;
	~RectTransform() = default;
	void Init(const InitDesc& desc) {
		m_anchoredPosition = desc.anchoredPosition;
		m_pivot = desc.pivot;
		m_sizeDelta = desc.sizeDelta;
		Transform::Init(desc);
	}

	void UpdateGeometry() override;	// Update the geometry of the RectTransform based on its properties (anchored position, pivot, size delta)

	// Setters
	void SetAnchoredPosition(const Vector2& anchoredPosition) { m_anchoredPosition = anchoredPosition; MarkDirty(); }	// Set the anchored position and mark as dirty
	void SetPivot(const Vector2& pivot) { m_pivot = pivot; MarkDirty(); }												// Set the pivot and mark as dirty
	void SetSizeDelta(const Vector2& sizeDelta) { m_sizeDelta = sizeDelta; MarkDirty(); }								// Set the size delta and mark as dirty

	// Getters
	const Vector2& GetAnchoredPosition() const { return m_anchoredPosition; }	// Get the anchored position
	const Vector2& GetPivot() const { return m_pivot; }							// Get the pivot
	const Vector2& GetSizeDelta() const { return m_sizeDelta; }					// Get the size delta

private:
	Vector2 m_anchoredPosition = Vector2::Zero();	// Position relative to the anchor point
	Vector2 m_pivot = Vector2(0.5f, 0.5f);			// Pivot point (normalized, (0,0) is top-left, (1,1) is bottom-right)
	Vector2 m_sizeDelta = Vector2::Zero();			// Size difference from the original size (used for scaling)

private:
	// Overrides
	void OnStartOverride() override {};
	void PreUpdateOverride(float deltaTime) override {};
	void UpdateOverride(float deltaTime) override {};
	void LateUpdateOverride(float deltaTime) override {};
	void OnDestroyOverride() override {};
};