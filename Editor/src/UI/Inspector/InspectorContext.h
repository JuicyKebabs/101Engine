#pragma once
#include <functional>
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Core/Math/Math.h"

//------------------------------------------------------------------
// InspectorContext
// Context to contain information for drawing inspector UI elements.
//------------------------------------------------------------------

class AssetManager;

struct InspectorContext
{
	AssetManager* assetManager = nullptr;	// For drawing asset pull-downs and asset previews.

	// Callback when a transform edit begins (user starts editing)
	std::function<void(const Guid& actorGuid, const Transform3D& before)> onTransformEditBegin;

	// Callback when a transform edit ends (user finishes editing)
	std::function<void(const Guid& actorGuid, const Transform3D& after)> onTransformEditEnd;

	// Callback when a transform edit is canceled (user cancels editing)
	std::function<void()> onCancelTransformEdit;
};
