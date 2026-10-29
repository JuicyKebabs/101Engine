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
    RenderView effectiveView;
    detail::RenderList objectIndices;
    // Empty means every draw uses effectiveView.drawSettings. An auxiliary
    // list has one entry per index, so repeated indices can use different Materials.
    std::vector<DrawSettings> perDrawSettings;
    std::string passName;
    backend::DepthTexture* depthOverride = nullptr;
    backend::DepthTexture* sampledShadow = nullptr;
    Matrix4x4 shadowViewProjection = Matrix4x4::Identity();
    bool shadowEnabled = false;
    bool depthOnly = false;
    bool loadColor = false;
    bool loadDepth = false;
    std::uint32_t widthOverride = 0;
    std::uint32_t heightOverride = 0;

    const DrawSettings& SettingsFor(std::size_t drawIndex) const
    {
        if (!perDrawSettings.empty())
        {
            return perDrawSettings[drawIndex];
        }

        return effectiveView.drawSettings;
    }
};

bool SameViewOutput(const ViewOutput& left, const ViewOutput& right) noexcept;
Result ValidateViewStructure(const FrameRenderData& frame);

class FrameRenderer
{
public:
    Result SelectViewDraws(const FrameRenderData& frame, const RenderView& view,
        detail::FrameDrawContext& context, detail::RenderList& indices) const;
    Result SelectViews(FrameRenderData& frame, detail::FrameDrawContext& context,
        std::vector<ViewRenderList>& lists) const;
    Result BuildViews(const FrameRenderData& frame, detail::FrameDrawContext& context,
        const std::vector<ViewRenderList>& lists, graph::RenderGraph& graph) const;

    // Existing manual Graph checks keep a small one-View entry.
    Result SelectSceneDraws(const FrameRenderData& frame, detail::FrameDrawContext& context,
        detail::RenderList& indices) const;
    Result Build(const FrameRenderData& frame, detail::FrameDrawContext& context,
        const detail::RenderList& indices, graph::RenderGraph& graph) const;

private:
    Result AddStepPass(const FrameRenderData& frame, const ViewRenderList& list,
        detail::FrameDrawContext& context, graph::RenderGraph& graph) const;

};

}
