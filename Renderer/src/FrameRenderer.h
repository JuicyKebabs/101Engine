// R7 implementation guide: section 4.31.
// ViewRenderList stores source indices only; FrameRenderData continues owning Object values.
// Scene selection/sorting and Overlay order produce explicit Pass declarations.
// The generated order is deliberate, not a new general-purpose pass scheduler.

#pragma once

#include "FrameDrawContext.h"

namespace rendering
{

struct ViewRenderList
{
    std::size_t viewIndex = 0;
    detail::RenderList objectIndices;
};

bool SameViewOutput(const ViewOutput& left, const ViewOutput& right) noexcept;
Result ValidateViewStructure(const FrameRenderData& frame);

class FrameRenderer
{
public:
    Result SelectViewDraws(const FrameRenderData& frame, const RenderView& view,
        detail::FrameDrawContext& context, detail::RenderList& indices) const;
    Result SelectViews(const FrameRenderData& frame, detail::FrameDrawContext& context,
        std::vector<ViewRenderList>& lists) const;
    Result BuildViews(const FrameRenderData& frame, detail::FrameDrawContext& context,
        const std::vector<ViewRenderList>& lists, graph::RenderGraph& graph) const;

    // Existing manual Graph checks keep a small one-View entry.
    Result SelectSceneDraws(const FrameRenderData& frame, detail::FrameDrawContext& context,
        detail::RenderList& indices) const;
    Result Build(const FrameRenderData& frame, detail::FrameDrawContext& context,
        const detail::RenderList& indices, graph::RenderGraph& graph) const;

private:
    Result AddViewPass(const RenderView& view, const detail::RenderList& indices,
        detail::FrameDrawContext& context, graph::RenderGraph& graph, bool loadColor) const;
};

}
