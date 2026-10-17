#pragma once

#include "ResourceHandle.h"
#include "WindowOutput.h"

#include <vector>

//----------------------------------------------------------------------------------------
// FrameRenderData
// This structure contains the data for a single frame to be rendered.
// User construct this data before calling Render() and submit this data to the Renderer.
// Only single submission is accepted in a frame. 
// The Renderer will consume the submission in the next Render() call.
//----------------------------------------------------------------------------------------

namespace rendering
{

class Renderer;

struct RenderObject
{
	GeometryHandle geometry;
	TextureHandle texture;	// Initial direct reference; move to Material when it is introduced.
};

struct RenderView
{
	WindowOutputHandle output;
};

struct FrameRenderData
{
	std::vector<RenderObject> objects;
	std::vector<RenderView> views;

private:
	friend class Renderer;

	// Accepted but not yet consumed. This does not indicate CPU or GPU completion.
	bool hasPendingSubmission = false;
};

}
