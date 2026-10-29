// R7 implementation guide: section 4.32.
// Build Scene passes first and Overlay passes afterwards, preserving order within each group.
// Scene owns clear/Depth behavior; Overlay preserves UI submission order and has no Depth attachment.
// A current-frame output cannot be read before its producer, even if last frame had valid pixels.

#include "FrameRenderer.h"
#include "Failure.h"
#include "SceneFrameResources.h"
#include "Graph/RenderPassContext.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

    // Auxiliary source IDs must refer to normal Views, so a recursive source
    // graph cannot be expressed. The array supplies order within each source.
    for (std::size_t passIndex = 0; passIndex < frame.auxiliaryPasses.size(); ++passIndex)
    {
        const AuxiliaryPass& pass = frame.auxiliaryPasses[passIndex];
        const auto source = std::find_if(frame.views.begin(), frame.views.end(),
            [&pass](const RenderView& view) { return view.id == pass.sourceViewId; });
        if (source == frame.views.end())
        {
            return detail::Fail("Auxiliary source View does not exist.");
        }

        const bool sceneStage = source->kind == ViewKind::Scene && pass.stage == AuxiliaryStage::BeforeDisplayTransform;
        const bool overlayStage = source->kind == ViewKind::Overlay && pass.stage == AuxiliaryStage::AfterOverlay;
        if (!sceneStage && !overlayStage)
        {
            return detail::Fail("Auxiliary insertion stage does not match its source View.");
        }

        if (pass.useSourceDepth && source->kind != ViewKind::Scene)
        {
            return detail::Fail("Overlay source has no Depth attachment to Load.");
        }

        if (pass.colorLoad != AuxiliaryColorLoad::Clear && pass.colorLoad != AuxiliaryColorLoad::Load)
        {
            return detail::Fail("Unknown auxiliary color load operation.");
        }

        const bool ordinaryCollision = std::any_of(frame.views.begin(), frame.views.end(),
            [&pass](const RenderView& view) { return view.id == pass.id; });
        if (ordinaryCollision)
        {
            return detail::Fail("Auxiliary IDs must be distinct from normal View IDs.");
        }

        for (std::size_t previous = 0; previous < passIndex; ++previous)
        {
            if (frame.auxiliaryPasses[previous].id == pass.id)
            {
                return detail::Fail("Duplicate auxiliary Pass ID.");
            }
        }

        for (float channel : pass.clearColor)
        {
            if (!std::isfinite(channel))
            {
                return detail::Fail("Auxiliary clear color must be finite.");
            }
        }

        for (const AuxiliaryDraw& draw : pass.draws)
        {
            if (draw.objectIndex >= frame.objects.size())
            {
                return detail::Fail("Auxiliary Draw references an out-of-range original Object index.");
            }
        }
    }

    for (const auto& object : frame.objects)
    {
        for (ViewId targetId : object.targetViews)
        {
            const bool exists = std::any_of(frame.views.begin(), frame.views.end(),
                [targetId](const RenderView& view) { return view.id == targetId; });
            const bool auxiliaryExists = std::any_of(frame.auxiliaryPasses.begin(), frame.auxiliaryPasses.end(),
                [targetId](const AuxiliaryPass& pass) { return pass.id == targetId; });
            if (!exists && !auxiliaryExists)
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

namespace
{

// Configure both selection and recording from the same plan entry. The Light
// projection may differ from the Camera used to derive Billboard World.
void ConfigureStep(const FrameRenderData& frame, const ViewRenderList& list,
    detail::FrameDrawContext& context)
{
    context.SetActiveView(list.effectiveView.id);
    context.SetSceneInputs(&frame.views[list.viewIndex], list.sampledShadow,
        list.shadowViewProjection, list.shadowEnabled);
    context.SetRecordingExtent(list.widthOverride, list.heightOverride);
}

Result ReserveInternalId(const FrameRenderData& frame, const std::vector<ViewRenderList>& lists, ViewId& output)
{
    ViewId candidate = (std::numeric_limits<ViewId>::max)();
    for (;;)
    {
        const bool ordinaryUse = std::any_of(frame.views.begin(), frame.views.end(),
            [candidate](const RenderView& view) { return view.id == candidate; });
        const bool auxiliaryUse = std::any_of(frame.auxiliaryPasses.begin(), frame.auxiliaryPasses.end(),
            [candidate](const AuxiliaryPass& pass) { return pass.id == candidate; });
        const bool internalUse = std::any_of(lists.begin(), lists.end(),
            [candidate](const ViewRenderList& list) { return list.effectiveView.id == candidate; });
        if (!ordinaryUse && !auxiliaryUse && !internalUse)
        {
            output = candidate;
            return {};
        }

        if (candidate == 0)
        {
            return detail::Fail("No Frame-local ID remains for the private display transform.");
        }

        --candidate;
    }
}

RenderView MakeShadowView(const RenderView& source, const DirectionalLight& light,
    RenderTargetTextureHandle hdrOutput)
{
    Vector3 direction;
    direction.x = light.lightDirection[0];
    direction.y = light.lightDirection[1];
    direction.z = light.lightDirection[2];
    direction = direction.Normalized();

    Vector3 up = Vector3::Up();
    if (std::abs(direction.Dot(up)) > 0.95f)
    {
        up = Vector3::Right();
    }

    const Vector3 eye = source.cameraPosition - direction * 80.0f;
    RenderView shadow = source;
    shadow.output = hdrOutput; // Only an extent lookup; the Pass writes Depth only.
    shadow.view = Matrix4x4::CreateLookAt(eye, source.cameraPosition, up);
    shadow.projection = Matrix4x4::CreateOrthographic(40, 40, 0.1f, 400);
    shadow.cameraPosition = eye;
    Viewport viewport;
    viewport.width = 4096;
    viewport.height = 4096;
    shadow.viewport = viewport;
    return shadow;
}

}

Result FrameRenderer::SelectViews(FrameRenderData& frame, detail::FrameDrawContext& context,
    std::vector<ViewRenderList>& lists) const
{
    // Ordinary source Views remain stable. Internal full-screen Objects append
    // to the same owning array and use private IDs, so they cannot enter Scene
    // selection or the host's explicit auxiliary lists by accident.
    for (ViewKind kind : { ViewKind::Scene, ViewKind::Overlay })
    {
        for (std::size_t viewIndex = 0; viewIndex < frame.views.size(); ++viewIndex)
        {
            const RenderView& source = frame.views[viewIndex];
            if (source.kind != kind)
            {
                continue;
            }

            ViewRenderList sceneList;
            sceneList.viewIndex = viewIndex;
            sceneList.effectiveView = source;
            if (kind == ViewKind::Overlay)
            {
                // Overlay ordering is already determined by its submitter.
                // There is no Depth attachment even for a Canvas preview of a
                // normally world-space Material. Compile an explicit PSO variant.
                sceneList.effectiveView.drawSettings.depthOverride = DepthMode::Disabled;
            }
            sceneList.passName = (kind == ViewKind::Scene ? "SceneHDR[" : "Overlay[") + std::to_string(source.id) + "]";
            sceneList.sampledShadow = &context.DisabledShadow();
            detail::SceneOutputImages* images = nullptr;
            if (kind == ViewKind::Scene)
            {
                Result shadowPreparation;
                Result prepared = context.PrepareSceneImages(source, images, shadowPreparation);
                if (!prepared.IsSuccess())
                {
                    return prepared;
                }

                sceneList.effectiveView.output = images->hdrColor;
                if (frame.directionalLight && shadowPreparation.IsSuccess() && images->shadowDepth)
                {
                    ViewRenderList shadowList;
                    shadowList.viewIndex = viewIndex;
                    shadowList.effectiveView = MakeShadowView(source, *frame.directionalLight, images->hdrColor);
                    shadowList.effectiveView.drawSettings.pipelineOverride = context.ShadowPipeline();
                    shadowList.effectiveView.drawSettings.materialOverride.reset();
                    shadowList.passName = "Shadow[" + std::to_string(source.id) + "]";
                    shadowList.depthOnly = true;
                    shadowList.depthOverride = images->shadowDepth.get();
                    shadowList.sampledShadow = &context.DisabledShadow();
                    shadowList.widthOverride = 4096;
                    shadowList.heightOverride = 4096;
                    ConfigureStep(frame, shadowList, context);

                    // This selection starts from all source members under Light
                    // projection. Camera visibility never determines casters.
                    Result selected = context.SelectAvailableDraws(shadowList.effectiveView.drawSettings,
                        shadowList.objectIndices, &shadowList.effectiveView);
                    if (!selected.IsSuccess())
                    {
                        return selected;
                    }

                    std::erase_if(shadowList.objectIndices,
                        [&frame](std::size_t index) { return !frame.objects[index].castShadow; });
                    sceneList.sampledShadow = images->shadowDepth.get();
                    sceneList.shadowViewProjection = shadowList.effectiveView.view * shadowList.effectiveView.projection;
                    sceneList.shadowEnabled = true;
                    lists.push_back(std::move(shadowList));
                }
                else if (!shadowPreparation.IsSuccess())
                {
                    // Preparation failed before any Shadow usage declaration.
                    // Keep a fresh disabled binding, never an old Frame's map.
                    context.ReportViewFailure(source.id, "ShadowPrepare", shadowPreparation.message);
                }
            }

            ConfigureStep(frame, sceneList, context);
            Result selected = SelectViewDraws(frame, sceneList.effectiveView, context, sceneList.objectIndices);
            if (!selected.IsSuccess())
            {
                return selected;
            }

            // Overlay loads a previously produced output. A first Overlay still
            // clears its output, including the no-Camera/background-only case.
            sceneList.loadColor = kind == ViewKind::Overlay && std::any_of(lists.begin(), lists.end(),
                [&source](const ViewRenderList& previous)
                {
                    return !previous.depthOnly && SameViewOutput(previous.effectiveView.output, source.output);
                });
            const Matrix4x4 sourceShadowMatrix = sceneList.shadowViewProjection;
            backend::DepthTexture* sourceShadow = sceneList.sampledShadow;
            const bool sourceShadowEnabled = sceneList.shadowEnabled;
            lists.push_back(std::move(sceneList));

            for (const AuxiliaryPass& pass : frame.auxiliaryPasses)
            {
                if (pass.sourceViewId != source.id)
                {
                    continue;
                }

                ViewRenderList auxiliary;
                auxiliary.viewIndex = viewIndex;
                auxiliary.effectiveView = source;
                auxiliary.effectiveView.id = pass.id;
                auxiliary.effectiveView.kind = ViewKind::Overlay;
                DrawSettings auxiliarySettings;
                auxiliary.effectiveView.drawSettings = auxiliarySettings;
                auxiliary.effectiveView.clearColor = pass.clearColor;
                if (pass.colorOutput)
                {
                    auxiliary.effectiveView.output = *pass.colorOutput;
                }
                else if (images)
                {
                    auxiliary.effectiveView.output = images->hdrColor;
                }

                auxiliary.passName = "Auxiliary[" + std::to_string(pass.id) + "] " + pass.name;
                auxiliary.loadColor = pass.colorLoad == AuxiliaryColorLoad::Load;
                auxiliary.loadDepth = pass.useSourceDepth;
                auxiliary.sampledShadow = sourceShadow;
                auxiliary.shadowViewProjection = sourceShadowMatrix;
                auxiliary.shadowEnabled = sourceShadowEnabled;
                if (pass.useSourceDepth)
                {
                    auxiliary.depthOverride = context.SceneDepth(*images);
                }

                // A borrowed Camera/Viewport/Depth is expressed in source pixel
                // coordinates. Reject incompatible auxiliary allocations before
                // Graph import; a color-only Mask must obey the same extent.
                std::uint32_t sourceWidth = 0;
                std::uint32_t sourceHeight = 0;
                std::uint32_t auxiliaryWidth = 0;
                std::uint32_t auxiliaryHeight = 0;
                Result extent = context.GetViewExtent(source, sourceWidth, sourceHeight);
                if (!extent.IsSuccess())
                {
                    return extent;
                }

                extent = context.GetViewExtent(auxiliary.effectiveView, auxiliaryWidth, auxiliaryHeight);
                if (!extent.IsSuccess())
                {
                    return extent;
                }

                if (sourceWidth != auxiliaryWidth || sourceHeight != auxiliaryHeight)
                {
                    return detail::Fail("Auxiliary output extent must match the borrowed source View.");
                }

                ConfigureStep(frame, auxiliary, context);
                for (const AuxiliaryDraw& draw : pass.draws)
                {
                    DrawSettings settings;
                    settings.materialOverride = draw.materialOverride;
                    if (!pass.useSourceDepth)
                    {
                        settings.depthOverride = DepthMode::Disabled;
                    }
                    if (context.SelectExplicitDraw(draw.objectIndex, settings))
                    {
                        auxiliary.objectIndices.push_back(draw.objectIndex);
                        auxiliary.perDrawSettings.push_back(settings);
                    }
                }

                lists.push_back(std::move(auxiliary));
            }

            if (images)
            {
                ViewRenderList display;
                display.viewIndex = viewIndex;
                display.effectiveView = source;
                display.effectiveView.kind = ViewKind::Overlay;
                // HDR stores an entire output, including its clear outside a
                // partial Scene viewport. Convert the complete image one-to-one.
                display.effectiveView.viewport.reset();
                DrawSettings displaySettings;
                display.effectiveView.drawSettings = displaySettings;
                Result reserved = ReserveInternalId(frame, lists, display.effectiveView.id);
                if (!reserved.IsSuccess())
                {
                    return reserved;
                }

                display.passName = "DisplayTransform[" + std::to_string(source.id) + "]";
                display.sampledShadow = &context.DisabledShadow();
                ProceduralGeometry fullscreen;
                fullscreen.vertexCount = 3;
                RenderObject object;
                object.material = images->displayMaterial;
                object.procedural = fullscreen;
                object.targetViews.push_back(display.effectiveView.id);
                const std::size_t objectIndex = frame.objects.size();
                frame.objects.push_back(std::move(object));
                display.objectIndices.push_back(objectIndex);
                lists.push_back(std::move(display));
            }
        }
    }

    return {};
}

Result FrameRenderer::AddStepPass(const FrameRenderData& frame, const ViewRenderList& list,
    detail::FrameDrawContext& context, graph::RenderGraph& graph) const
{
    ConfigureStep(frame, list, context);
    graph::ViewTargets targets;
    Result result;
    if (!list.depthOnly)
    {
        result = context.PrepareViewTargets(list.effectiveView, targets);
        if (!result.IsSuccess())
        {
            return result;
        }
    }

    if (list.depthOverride)
    {
        result = context.ImportDepth(*list.depthOverride, targets.depth);
        if (!result.IsSuccess())
        {
            return result;
        }
    }

    graph::RenderPassBuilder builder;
    result = builder.SetName(list.passName);
    if (!result.IsSuccess())
    {
        return result;
    }

    if (!list.depthOnly)
    {
        graph::ColorAttachment color;
        color.texture = targets.color;
        color.loadOp = list.loadColor ? graph::AttachmentLoadOp::Load : graph::AttachmentLoadOp::Clear;
        color.clearColor = list.effectiveView.clearColor;
        result = builder.ColorOutput(color);
        if (!result.IsSuccess())
        {
            return result;
        }
    }

    const bool includesDepth = list.depthOverride || list.effectiveView.kind == ViewKind::Scene;
    if (includesDepth)
    {
        graph::DepthAttachment depth;
        depth.texture = targets.depth;
        depth.loadOp = list.loadDepth ? graph::AttachmentLoadOp::Load : graph::AttachmentLoadOp::Clear;
        depth.clearDepth = 1;
        result = builder.DepthOutput(depth);
        if (!result.IsSuccess())
        {
            return result;
        }
    }

    for (std::size_t drawIndex = 0; drawIndex < list.objectIndices.size(); ++drawIndex)
    {
        detail::RenderList singleDraw;
        singleDraw.push_back(list.objectIndices[drawIndex]);
        result = context.DeclareDrawResources(builder, singleDraw, list.SettingsFor(drawIndex));
        if (!result.IsSuccess())
        {
            return result;
        }
    }

    // The graph owns the index/settings plan. Manual Build callers may return
    // before Execute; borrowing a local ViewRenderList would dangle there.
    result = builder.SetExecute([&frame, list, &context](graph::RenderPassContext& pass)
    {
        ConfigureStep(frame, list, context);
        for (std::size_t drawIndex = 0; drawIndex < list.objectIndices.size(); ++drawIndex)
        {
            detail::RenderList singleDraw;
            singleDraw.push_back(list.objectIndices[drawIndex]);
            Result recorded = context.RecordRenderList(pass, list.effectiveView, singleDraw, list.SettingsFor(drawIndex));
            if (!recorded.IsSuccess())
            {
                return recorded;
            }
        }

        return Result{};
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

Result FrameRenderer::BuildViews(const FrameRenderData& frame, detail::FrameDrawContext& context,
    const std::vector<ViewRenderList>& lists, graph::RenderGraph& graph) const
{
    // A known current-frame clear supplies a valid binding for disabled shadows.
    // A graph execution/submission failure still aborts the Frame as a whole.
    graph::GraphTextureHandle disabledShadow;
    Result result = context.ImportDepth(context.DisabledShadow(), disabledShadow);
    if (!result.IsSuccess())
    {
        return result;
    }

    graph::DepthAttachment disabledDepth;
    disabledDepth.texture = disabledShadow;
    disabledDepth.loadOp = graph::AttachmentLoadOp::Clear;
    disabledDepth.clearDepth = 1;
    graph::RenderPassBuilder disabledBuilder;
    result = disabledBuilder.SetName("InitializeDisabledShadow");
    if (result.IsSuccess())
    {
        result = disabledBuilder.DepthOutput(disabledDepth);
    }
    if (result.IsSuccess())
    {
        result = disabledBuilder.SetExecute([](graph::RenderPassContext&) { return Result{}; });
    }
    if (!result.IsSuccess())
    {
        return result;
    }

    std::optional<graph::RenderPass> disabledPass;
    result = std::move(disabledBuilder).Build(disabledPass);
    if (!result.IsSuccess())
    {
        return result;
    }

    result = graph.AddPass(std::move(*disabledPass));
    if (!result.IsSuccess())
    {
        return result;
    }

    std::vector<RenderTargetTextureHandle> producers;
    for (const ViewRenderList& list : lists)
    {
        ConfigureStep(frame, list, context);
        for (std::size_t drawIndex = 0; drawIndex < list.objectIndices.size(); ++drawIndex)
        {
            detail::RenderList singleDraw;
            singleDraw.push_back(list.objectIndices[drawIndex]);
            result = context.CheckOutputReadOrder(singleDraw, list.SettingsFor(drawIndex), frame, producers);
            if (!result.IsSuccess())
            {
                return result;
            }
        }

        result = AddStepPass(frame, list, context, graph);
        if (!result.IsSuccess())
        {
            return result;
        }

        if (!list.depthOnly)
        {
            if (const auto* target = std::get_if<RenderTargetTextureHandle>(&list.effectiveView.output))
            {
                producers.push_back(*target);
            }
        }
    }

    return {};
}

// Narrow manual-Graph test entry points remain useful. Production Render uses
// the complete R8 plan above, with no old Engine drawing backend or mode switch.
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

    ViewRenderList list;
    list.effectiveView = frame.views.front();
    list.objectIndices = indices;
    list.passName = "ManualScene";
    return AddStepPass(frame, list, context, graph);
}

}
