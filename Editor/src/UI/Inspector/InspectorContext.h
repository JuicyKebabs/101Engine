#pragma once

//------------------------------------------------------------------
// InspectorContext
// Context to contain information for drawing inspector UI elements.
//------------------------------------------------------------------

class AssetManager;

struct InspectorContext
{
	AssetManager* assetManager = nullptr;	// For drawing asset pull-downs and asset previews.
};