#pragma once

#include "Renderer/FrameRenderData.h"
#include "Graph/RenderPass.h"

#include <optional>
#include <span>

namespace rendering::backend
{
class DepthTexture;
}

namespace rendering
{

struct Extent2D
{
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

struct PlannedDraw
{
	std::size_t objectIndex = 0;
	DrawSettings settings;
};

struct ShadowInputs
{
	backend::DepthTexture* texture = nullptr;
	Matrix4x4 viewProjection = Matrix4x4::Identity();
	bool enabled = false;
};

// Pass-local borrows. The owning frame and plan outlive declaration/recording.
struct PassRecordingInputs
{
	const RenderView* view = nullptr;
	const RenderView* billboardSource = nullptr;
	ShadowInputs shadow;
	ViewId viewId = 0;
	std::string passName;
	std::optional<Extent2D> extentOverride;
	Extent2D extent;
	Viewport viewport;
};

struct PlannedPass
{
	std::size_t sourceViewIndex = 0;
	RenderView renderView;
	std::string name;
	std::vector<PlannedDraw> draws;
	backend::DepthTexture* depth = nullptr;
	ShadowInputs shadow;
	std::optional<Extent2D> extentOverride;
	std::optional<graph::AttachmentLoadOp> colorLoad;
	std::optional<graph::AttachmentLoadOp> depthLoad;
};

PassRecordingInputs MakePassRecordingInputs(const FrameRenderData& frame, const PlannedPass& pass);

} // namespace rendering
