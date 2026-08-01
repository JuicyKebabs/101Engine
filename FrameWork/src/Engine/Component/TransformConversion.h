#pragma once
#include <memory>
#include <optional>
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/Transform.h"

//----------------------------------------------------------------------
// TransformConversion
// Controls conversion between Transform and RectTransform.
// This conversion is used to satisfy UI hierarchy constraints.
// ---------------------------------------------------------------------

// Identifies the concrete Transform-family type
enum class TransformKind
{
	Transform,
	RectTransform
};

// RectTransform-specific state used during conversion
struct RectTransformState
{
	AnchorMode anchorMode = AnchorMode::MiddleCenter;
	Vector2 anchoredPosition = Vector2::Zero();
	Vector2 pivot = Vector2(0.5f, 0.5f);
	Vector2 size = Vector2(100.0f, 100.0f);
};

// Serialization-independent temporary state used when converting
// between Transform-family component types
struct TransformSnapshot
{
	// The kind of Transform-family component that was captured(will be converted to new kind).
	TransformKind sourceKind = TransformKind::Transform;

	// Original common transform data
	Transform3D localTransform = Transform3D();

	// Actual local pose after applying RectTransform layout offsets.
	// For a normal Transform, this is identical to localTransform.
	Transform3D effectiveLocalTransform = Transform3D();

	std::string name;	// Name of the component

	// Optional RectTransform-specific state. Present only if the source was a RectTransform.
	std::optional<RectTransformState> rectTransformState;
};

// Creates a new Transform-family component without mutating an Actor
class TransformConversion
{
public:
	static TransformKind GetKind(const Transform& transform);

	// Create capture snapshot which can be used to create a new Transform-family component
	static TransformSnapshot Capture(const Transform& transform);

	// Create new specific kind of transform component from the snapshot
	static std::unique_ptr<Transform> Create(
		TransformKind targetKind,
		const TransformSnapshot& snapshot
	);
};
