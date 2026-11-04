// PlannedPass stores source indices only; FrameRenderData continues owning Object values.
// Scene selection/sorting and Overlay order produce explicit Pass declarations.
// The generated order is deliberate, not a new general-purpose pass scheduler.

#pragma once

#include "DrawPlan.h"

namespace rendering::detail
{
class FrameDrawContext;
}
namespace rendering::graph
{
class RenderGraph;
}

namespace rendering
{

bool SameViewOutput(const ViewOutput& left, const ViewOutput& right) noexcept;
Result ValidateViewStructure(const FrameRenderData& frame);

class FrameRenderer
{
public:
	Result SelectViewDraws(const FrameRenderData& frame, const PassRecordingInputs& pass,
                           detail::FrameDrawContext& context, std::vector<std::size_t>& indices) const;
	Result BuildPassPlan(FrameRenderData& frame, detail::FrameDrawContext& context,
                         std::vector<PlannedPass>& lists) const;
	Result BuildRenderPasses(const FrameRenderData& frame, detail::FrameDrawContext& context,
                      const std::vector<PlannedPass>& lists, graph::RenderGraph& graph) const;

private:
	Result AddStepPass(const FrameRenderData& frame, const PlannedPass& list, detail::FrameDrawContext& context,
                       graph::RenderGraph& graph) const;
};

} // namespace rendering
