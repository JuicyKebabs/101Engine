// R7 implementation guide: section 4.32.
// Build Scene passes first and Overlay passes afterwards, preserving order within each group.
// Scene owns clear/Depth behavior; Overlay preserves UI submission order and has no Depth attachment.
// A current-frame output cannot be read before its producer, even if last frame had valid pixels.

#include "FrameRenderer.h"
#include "Failure.h"
#include "Graph/RenderPassContext.h"

#include <algorithm>
#include <cmath>

namespace rendering
{

bool SameViewOutput(const ViewOutput& left, const ViewOutput& right) noexcept
{
    if (left.index() != right.index())
    {
        return false;
    }

    if (const auto* window = std::get_if<WindowOutputHandle>(&left))
    {
        return window->value == std::get<WindowOutputHandle>(right).value;
    }

    return std::get<RenderTargetTextureHandle>(left) == std::get<RenderTargetTextureHandle>(right);
}

// Frame-local View IDs must be unique; referenced target IDs must exist.
// Only one Scene may clear/write each color output, making Depth and clear ordering unambiguous.
// Overlay may later load that output without introducing another Scene Depth owner.
Result ValidateViewStructure(const FrameRenderData& frame)
{
    for (std::size_t index = 0; index < frame.views.size(); ++index)
    {
        const auto& view = frame.views[index];
        if (view.kind != ViewKind::Scene && view.kind != ViewKind::Overlay)
        {
            return detail::Fail("Submit: invalid View kind.");
        }

        for (std::size_t previous = 0; previous < index; ++previous)
        {
            const auto& other = frame.views[previous];
            if (view.id == other.id)
            {
                return detail::Fail("Submit: View IDs must be unique within a Frame.");
            }

            const bool bothScene = view.kind == ViewKind::Scene && other.kind == ViewKind::Scene;
            if (bothScene && SameViewOutput(view.output, other.output))
            {
                return detail::Fail("Submit: only one Scene View may write each output.");
            }
        }

        for (float component : view.clearColor)
        {
            if (!std::isfinite(component))
            {
                return detail::Fail("Submit: clear color must be finite.");
            }
        }
    }

    for (const auto& object : frame.objects)
    {
        for (ViewId targetId : object.targetViews)
        {
            const bool exists = std::any_of(frame.views.begin(), frame.views.end(),
                [targetId](const RenderView& view) { return view.id == targetId; });
            if (!exists)
            {
                return detail::Fail("Submit: Object references an unknown View ID.");
            }
        }
    }

    return {};
}

// Classify using the final Pipeline's blend mode after View overrides.
// Opaque draws preserve source order; transparent draws sort by Camera-space z back-to-front.
// Stable sort keeps submission order for equal depth. Overlay deliberately bypasses this Scene ordering.
Result FrameRenderer::SelectViewDraws(const FrameRenderData& frame, const RenderView& view,
    detail::FrameDrawContext& context, detail::RenderList& indices) const
{
    detail::RenderList available;
    Result result = context.SelectAvailableDraws(view.drawSettings, available, &view);
    if (!result.IsSuccess())
    {
        return result;
    }

    if (view.kind == ViewKind::Overlay)
    {
        indices = std::move(available);
        return {};
    }

    detail::RenderList opaque;
    detail::RenderList transparent;
    for (std::size_t objectIndex : available)
    {
        if (context.UsesAlphaBlending(objectIndex, view.drawSettings))
        {
            transparent.push_back(objectIndex);
        }
        else
        {
            opaque.push_back(objectIndex);
        }
    }

    // Use the View matrix, not world Z or another View's Camera.
    // This is Object-origin sorting, not a general solution for intersecting transparent triangles.
    std::stable_sort(transparent.begin(), transparent.end(),
        [&frame, &view](std::size_t left, std::size_t right)
        {
            const auto leftPosition = frame.objects[left].world.GetTranslation();
            const auto rightPosition = frame.objects[right].world.GetTranslation();
            return view.view.TransformPoint(leftPosition).z > view.view.TransformPoint(rightPosition).z;
        });
    opaque.insert(opaque.end(), transparent.begin(), transparent.end());
    indices = std::move(opaque);
    return {};
}

// Build stable Scene groups before Overlay groups, independent of mixed input ordering.
// Each list borrows its View via an index and retains only original Object indices.
Result FrameRenderer::SelectViews(const FrameRenderData& frame, detail::FrameDrawContext& context,
    std::vector<ViewRenderList>& lists) const
{
    for (ViewKind kind : { ViewKind::Scene, ViewKind::Overlay })
    {
        for (std::size_t index = 0; index < frame.views.size(); ++index)
        {
            if (frame.views[index].kind != kind)
            {
                continue;
            }

            ViewRenderList selected;
            selected.viewIndex = index;
            Result result = SelectViewDraws(frame, frame.views[index], context, selected.objectIndices);
            if (!result.IsSuccess())
            {
                return result;
            }
            lists.push_back(std::move(selected));
        }
    }
    return {};
}

// Declare attachments and sampled resources before installing the recording callback.
// The callback borrows View/context/index storage that outlives Execute within Renderer::Render.
// Clear affects the full attachment; a partial viewport does not turn it into a partial Clear.
Result FrameRenderer::AddViewPass(const RenderView& view, const detail::RenderList& indices,
    detail::FrameDrawContext& context, graph::RenderGraph& graph, bool loadColor) const
{
    graph::ViewTargets targets;
    Result result = context.PrepareViewTargets(view, targets);
    if (!result.IsSuccess())
    {
        return result;
    }

    graph::RenderPassBuilder builder;
    const std::string passName = (view.kind == ViewKind::Scene ? "Scene[" : "Overlay[") + std::to_string(view.id) + "]";
    result = builder.SetName(passName);
    if (!result.IsSuccess())
    {
        return result;
    }

    graph::ColorAttachment color;
    color.texture = targets.color;
    color.loadOp = loadColor ? graph::AttachmentLoadOp::Load : graph::AttachmentLoadOp::Clear;
    color.clearColor = view.clearColor;
    result = builder.ColorOutput(color);
    if (!result.IsSuccess())
    {
        return result;
    }

    if (view.kind == ViewKind::Scene)
    {
        graph::DepthAttachment depth;
        depth.texture = targets.depth;
        depth.loadOp = graph::AttachmentLoadOp::Clear;
        depth.clearDepth = 1;
        result = builder.DepthOutput(depth);
        if (!result.IsSuccess())
        {
            return result;
        }
    }

    result = context.DeclareDrawResources(builder, indices, view.drawSettings);
    if (!result.IsSuccess())
    {
        return result;
    }

    result = builder.SetExecute([&context, &view, &indices](graph::RenderPassContext& pass)
        {
            return context.RecordRenderList(pass, view, indices, view.drawSettings);
        });
    if (!result.IsSuccess())
    {
        return result;
    }

    std::optional<graph::RenderPass> pass;
    result = std::move(builder).Build(pass);
    if (!result.IsSuccess())
    {
        return result;
    }
    return graph.AddPass(std::move(*pass));
}

// Track which outputs have already been written in this frame.
// Only a later Overlay loads existing color; the first write clears.
// Check producer order before adding each Pass; Graph still rejects same-pass feedback or invalid reads.
Result FrameRenderer::BuildViews(const FrameRenderData& frame, detail::FrameDrawContext& context,
    const std::vector<ViewRenderList>& lists, graph::RenderGraph& graph) const
{
    std::vector<ViewOutput> writtenOutputs;
    std::vector<RenderTargetTextureHandle> completedProducers;
    for (const auto& list : lists)
    {
        const auto& view = frame.views[list.viewIndex];
        Result result = context.CheckOutputReadOrder(list.objectIndices, view.drawSettings,
            frame, completedProducers);
        if (!result.IsSuccess())
        {
            return result;
        }

        const bool alreadyWritten = std::any_of(writtenOutputs.begin(), writtenOutputs.end(),
            [&view](const ViewOutput& output) { return SameViewOutput(output, view.output); });
        const bool loadColor = view.kind == ViewKind::Overlay && alreadyWritten;
        result = AddViewPass(view, list.objectIndices, context, graph, loadColor);
        if (!result.IsSuccess())
        {
            return result;
        }

        writtenOutputs.push_back(view.output);
        if (const auto* target = std::get_if<RenderTargetTextureHandle>(&view.output))
        {
            completedProducers.push_back(*target);
        }
    }
    return {};
}

Result FrameRenderer::SelectSceneDraws(const FrameRenderData& frame, detail::FrameDrawContext& context,
    detail::RenderList& indices) const
{
    if (frame.views.empty())
    {
        DrawSettings settings;
        return context.SelectAvailableDraws(settings, indices);
    }
    return SelectViewDraws(frame, frame.views.front(), context, indices);
}

Result FrameRenderer::Build(const FrameRenderData& frame, detail::FrameDrawContext& context,
    const detail::RenderList& indices, graph::RenderGraph& graph) const
{
    if (frame.views.empty())
    {
        return {};
    }
    return AddViewPass(frame.views.front(), indices, context, graph, false);
}

}
