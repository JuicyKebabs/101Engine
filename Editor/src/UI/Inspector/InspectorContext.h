#pragma once
#include <functional>
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Core/Math/Math.h"
#include "Command/RectTransformEditCommand.h"

//------------------------------------------------------------------
// InspectorContext
// Context to contain information for drawing inspector UI elements.
//------------------------------------------------------------------

class AssetManager;

struct InspectorContext
{
	AssetManager* assetManager = nullptr;	// For drawing asset pull-downs and asset previews.

	// Transform editing events.
	std::function<void(const Guid& actorGuid, const Transform3D& before)> onTransformEditBegin;
	std::function<void(const Guid& actorGuid, const Transform3D& after)> onTransformEditEnd;
	std::function<void()> onCancelTransformEdit;

	// RectTransform editing events.
	std::function<void(const Guid& actorGuid, const RectTransformEditState& before)> onRectTransformEditBegin;
	std::function<void(const Guid& actorGuid, const RectTransformEditState& after)> onRectTransformEditEnd;
	std::function<void()> onCancelRectTransformEdit;
};
