// R7 implementation guide: section 4.32.
// Build Scene passes first and Overlay passes afterwards, preserving order within each group.
// Scene owns clear/Depth behavior; Overlay preserves UI submission order and has no Depth attachment.
// A current-frame output cannot be read before its producer, even if last frame had valid pixels.

#include "FrameRenderer.h"
#include "FrameDrawContext.h"
#include "Failure.h"
#include "SceneOutputResources.h"
#include "Graph/RenderPassContext.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace rendering
{

PassRecordingInputs MakePassRecordingInputs(const FrameRenderData& frame, const PlannedPass& pass)
{
	PassRecordingInputs inputs;
	inputs.view = &pass.renderView;
	inputs.billboardSource = &frame.views[pass.sourceViewIndex];
	inputs.shadow = pass.shadow;
	inputs.viewId = pass.renderView.id;
	inputs.passName = pass.name;
	inputs.extentOverride = pass.extentOverride;
	return inputs;
}

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

		// Compare with previous checked Views.
		for (std::size_t previous = 0; previous < index; ++previous)
		{
			const auto& other = frame.views[previous];

			// Duplicated views are not allowed.
			if (view.id == other.id)
			{
				return detail::Fail("Submit: View IDs must be unique within a Frame.");
			}

			// Multiple scene Views are not allowed.
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
		const auto source = std::find_if(
			frame.views.begin(), frame.views.end(),
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

		const bool ordinaryCollision = std::any_of(
			frame.views.begin(), frame.views.end(),
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
			const bool auxiliaryExists =
				std::any_of(frame.auxiliaryPasses.begin(), frame.auxiliaryPasses.end(),
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
Result FrameRenderer::SelectViewDraws(const FrameRenderData& frame, const PassRecordingInputs& pass,
                                      detail::FrameDrawContext& context, detail::RenderList& indices) const
{
	const RenderView& view = *pass.view;
	detail::RenderList available;
	Result result = context.SelectAvailableDraws(view.drawSettings, available, pass);

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
		if (context.UsesAlphaBlending(objectIndex, view.drawSettings, pass))
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
void AppendPlannedDraws(const detail::RenderList& indices, PlannedPass& pass)
{
	for (std::size_t objectIndex : indices)
	{
		PlannedDraw draw;
		draw.objectIndex = objectIndex;
		draw.settings = pass.renderView.drawSettings;
		pass.draws.push_back(draw);
	}
}

Result ReserveInternalId(const FrameRenderData& frame, const std::vector<PlannedPass>& lists, ViewId& output)
{
	ViewId candidate = (std::numeric_limits<ViewId>::max)();

	for (;;)
	{
		const bool ordinaryUse = std::any_of(frame.views.begin(), frame.views.end(),
                                             [candidate](const RenderView& view) { return view.id == candidate; });
		const bool auxiliaryUse = std::any_of(frame.auxiliaryPasses.begin(), frame.auxiliaryPasses.end(),
                                              [candidate](const AuxiliaryPass& pass) { return pass.id == candidate; });
		const bool internalUse = std::any_of(lists.begin(), lists.end(), [candidate](const PlannedPass& list)
                                             { return list.renderView.id == candidate; });

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

RenderView MakeShadowView(const RenderView& source, const DirectionalLight& light, RenderTargetTextureHandle hdrOutput)
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

	shadow.viewport.reset();

	return shadow;
}

Result AppendShadowPass(FrameRenderData& frame, detail::FrameDrawContext& context, detail::SceneOutputImages* images,
						PlannedPass& scene, std::vector<PlannedPass>& lists)
{
	const RenderView& source = frame.views[scene.sourceViewIndex];
	PlannedPass shadowList;
	shadowList.sourceViewIndex = scene.sourceViewIndex;
	shadowList.renderView = MakeShadowView(source, *frame.directionalLight, images->hdrColor);

	shadowList.renderView.drawSettings.pipelineOverride = context.ShadowPipeline();
	shadowList.renderView.drawSettings.materialOverride.reset();
	shadowList.name = "Shadow[" + std::to_string(source.id) + "]";

	shadowList.depthLoad = graph::AttachmentLoadOp::Clear;
	shadowList.depth = images->shadowDepth.get();
	shadowList.shadow.texture = &context.DisabledShadow();

	Extent2D shadowExtent;
	shadowExtent.width = static_cast<std::uint32_t>(images->shadowDepth->Resource()->GetDesc().Width);
	shadowExtent.height = images->shadowDepth->Resource()->GetDesc().Height;
	shadowList.extentOverride = shadowExtent;

	const PassRecordingInputs shadowInputs = MakePassRecordingInputs(frame, shadowList);
	detail::RenderList shadowIndices;

	// This selection starts from all source members under Light
	// projection. Camera visibility never determines casters.
	Result selected = context.SelectAvailableDraws(shadowList.renderView.drawSettings, shadowIndices, shadowInputs);

	if (!selected.IsSuccess())
	{
		return selected;
	}

	std::erase_if(shadowIndices, [&frame](std::size_t index) { return !frame.objects[index].castShadow; });
	AppendPlannedDraws(shadowIndices, shadowList);
	scene.shadow.texture = images->shadowDepth.get();
	scene.shadow.viewProjection = shadowList.renderView.view * shadowList.renderView.projection;
	scene.shadow.enabled = true;
	lists.push_back(std::move(shadowList));
	return {};
}

Result AppendAuxiliaryPasses(FrameRenderData& frame, detail::FrameDrawContext& context, std::size_t viewIndex,
                             detail::SceneOutputImages* images, const ShadowInputs& shadow,
                             std::vector<PlannedPass>& lists)
{
	const RenderView& source = frame.views[viewIndex];
	for (const AuxiliaryPass& pass : frame.auxiliaryPasses)
	{
		if (pass.sourceViewId != source.id)
		{
			continue;
		}

		PlannedPass auxiliary;
		auxiliary.sourceViewIndex = viewIndex;
		auxiliary.renderView = source;
		auxiliary.renderView.id = pass.id;
		auxiliary.renderView.kind = ViewKind::Overlay;

		DrawSettings auxiliarySettings;
		auxiliary.renderView.drawSettings = auxiliarySettings;

		auxiliary.renderView.clearColor = pass.clearColor;

		if (pass.colorOutput)
		{
			auxiliary.renderView.output = *pass.colorOutput;
		}
		else if (images)
		{
			auxiliary.renderView.output = images->hdrColor;
		}

		auxiliary.name = "Auxiliary[" + std::to_string(pass.id) + "] " + pass.name;
		auxiliary.colorLoad =
			pass.colorLoad == AuxiliaryColorLoad::Load ? graph::AttachmentLoadOp::Load : graph::AttachmentLoadOp::Clear;
		if (pass.useSourceDepth)
		{
			auxiliary.depthLoad = graph::AttachmentLoadOp::Load;
		}
		auxiliary.shadow.texture = shadow.texture;
		auxiliary.shadow.viewProjection = shadow.viewProjection;
		auxiliary.shadow.enabled = shadow.enabled;

		if (pass.useSourceDepth)
		{
			auxiliary.depth = context.SceneDepth(*images);
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

		extent = context.GetViewExtent(auxiliary.renderView, auxiliaryWidth, auxiliaryHeight);

		if (!extent.IsSuccess())
		{
			return extent;
		}

		if (sourceWidth != auxiliaryWidth || sourceHeight != auxiliaryHeight)
		{
			return detail::Fail("Auxiliary output extent must match the borrowed source View.");
		}

		const PassRecordingInputs auxiliaryInputs = MakePassRecordingInputs(frame, auxiliary);

		for (const AuxiliaryDraw& draw : pass.draws)
		{
			DrawSettings settings;
			settings.materialOverride = draw.materialOverride;

			if (!pass.useSourceDepth)
			{
				settings.depthOverride = DepthMode::Disabled;
			}

			if (context.SelectExplicitDraw(draw.objectIndex, settings, auxiliaryInputs))
			{
				PlannedDraw planned;
				planned.objectIndex = draw.objectIndex;
				planned.settings = settings;
				auxiliary.draws.push_back(planned);
			}
		}

		lists.push_back(std::move(auxiliary));
	}

	return {};
}

Result AppendDisplayPass(FrameRenderData& frame, detail::FrameDrawContext& context, std::size_t viewIndex,
                         detail::SceneOutputImages& images, std::vector<PlannedPass>& lists)
{
	const RenderView& source = frame.views[viewIndex];
	PlannedPass display;
	display.sourceViewIndex = viewIndex;
	display.renderView = source;
	display.colorLoad = graph::AttachmentLoadOp::Clear;
	display.renderView.kind = ViewKind::Overlay;

	// HDR stores an entire output, including its clear outside a
	// partial Scene viewport. Convert the complete image one-to-one.
	display.renderView.viewport.reset();

	DrawSettings displaySettings;
	display.renderView.drawSettings = displaySettings;

	Result reserved = ReserveInternalId(frame, lists, display.renderView.id);

	if (!reserved.IsSuccess())
	{
		return reserved;
	}

	display.name = "DisplayTransform[" + std::to_string(source.id) + "]";
	display.shadow.texture = &context.DisabledShadow();

	ProceduralGeometry fullscreen;
	fullscreen.vertexCount = 3;

	RenderObject object;
	object.material = images.displayMaterial;
	object.procedural = fullscreen;
	object.targetViews.push_back(display.renderView.id);

	const std::size_t objectIndex = frame.objects.size();
	frame.objects.push_back(std::move(object));
	PlannedDraw displayDraw;
	displayDraw.objectIndex = objectIndex;
	displayDraw.settings = display.renderView.drawSettings;
	display.draws.push_back(displayDraw);
	lists.push_back(std::move(display));

	return {};
}

} // namespace

Result FrameRenderer::BuildPassPlan(FrameRenderData& frame, detail::FrameDrawContext& context,
									std::vector<PlannedPass>& lists) const
{
	// Ordinary source Views remain stable. Internal full-screen Objects append
	// to the same owning array and use private IDs, so they cannot enter Scene
	// selection or the host's explicit auxiliary lists by accident.
	for (ViewKind kind : {ViewKind::Scene, ViewKind::Overlay})
	{
		for (std::size_t viewIndex = 0; viewIndex < frame.views.size(); ++viewIndex)
		{
			const RenderView& source = frame.views[viewIndex];

			if (source.kind != kind)
			{
				continue;
			}

			PlannedPass sceneList;
			sceneList.sourceViewIndex = viewIndex;
			sceneList.renderView = source;
			sceneList.colorLoad = graph::AttachmentLoadOp::Clear;

			if (kind == ViewKind::Scene)
			{
				sceneList.depthLoad = graph::AttachmentLoadOp::Clear;
			}

			if (kind == ViewKind::Overlay)
			{
				// Overlay ordering is already determined by its submitter.
				// There is no Depth attachment even for a Canvas preview of a
				// normally world-space Material. Compile an explicit PSO variant.
				sceneList.renderView.drawSettings.depthOverride = DepthMode::Disabled;
			}

			sceneList.name = (kind == ViewKind::Scene ? "SceneHDR[" : "Overlay[") + std::to_string(source.id) + "]";
			sceneList.shadow.texture = &context.DisabledShadow();

			detail::SceneOutputImages* images = nullptr;

			if (kind == ViewKind::Scene)
			{
				Result shadowPreparation;
				Result prepared = context.PrepareSceneImages(source, images, shadowPreparation);

				if (!prepared.IsSuccess())
				{
					return prepared;
				}

				sceneList.renderView.output = images->hdrColor;

				if (frame.directionalLight && shadowPreparation.IsSuccess() && images->shadowDepth)
				{

					Result shadow = AppendShadowPass(frame, context, images, sceneList, lists);

					if (!shadow.IsSuccess())
					{
						return shadow;
					}
				}
				else if (!shadowPreparation.IsSuccess())
				{
					// Preparation failed before any Shadow usage declaration.
					// Keep a fresh disabled binding, never an old Frame's map.
					context.ReportViewFailure(source.id, "ShadowPrepare", shadowPreparation.message);
				}
			}

			const PassRecordingInputs sceneInputs = MakePassRecordingInputs(frame, sceneList);
			detail::RenderList sceneIndices;
			Result selected = SelectViewDraws(frame, sceneInputs, context, sceneIndices);

			if (!selected.IsSuccess())
			{
				return selected;
			}

			// Overlay loads a previously produced output. A first Overlay still
			// clears its output, including the no-Camera/background-only case.
			const bool loadOverlay = kind == ViewKind::Overlay &&
                                     std::any_of(lists.begin(), lists.end(),
                                                 [&source](const PlannedPass& previous)
                                                 {
                                                     return previous.colorLoad.has_value() &&
															SameViewOutput(previous.renderView.output, source.output);
                                                 });

			sceneList.colorLoad = loadOverlay ? graph::AttachmentLoadOp::Load : graph::AttachmentLoadOp::Clear;
			AppendPlannedDraws(sceneIndices, sceneList);

			const ShadowInputs sourceShadow = sceneList.shadow;
			lists.push_back(std::move(sceneList));

			Result auxiliary = AppendAuxiliaryPasses(frame, context, viewIndex, images, sourceShadow, lists);

			if (!auxiliary.IsSuccess())
			{
				return auxiliary;
			}

			if (images)
			{
				Result display = AppendDisplayPass(frame, context, viewIndex, *images, lists);

				if (!display.IsSuccess())
				{
					return display;
				}
			}
		}
	}

	return {};
}

Result FrameRenderer::AddStepPass(const FrameRenderData& frame, const PlannedPass& list,
                                  detail::FrameDrawContext& context, graph::RenderGraph& graph) const
{
	const PassRecordingInputs inputs = MakePassRecordingInputs(frame, list);
	graph::ViewTargets targets;
	Result result;

	if (list.colorLoad.has_value())
	{
		result = context.PrepareViewTargets(list.renderView, targets);

		if (!result.IsSuccess())
		{
			return result;
		}
	}

	if (list.depth)
	{
		result = context.ImportDepth(*list.depth, targets.depth);

		if (!result.IsSuccess())
		{
			return result;
		}
	}

	graph::RenderPassBuilder builder;
	result = builder.SetName(list.name);

	if (!result.IsSuccess())
	{
		return result;
	}

	if (list.colorLoad.has_value())
	{
		graph::ColorAttachment color;
		color.texture = targets.color;
		color.loadOp = *list.colorLoad;
		color.clearColor = list.renderView.clearColor;
		result = builder.ColorOutput(color);

		if (!result.IsSuccess())
		{
			return result;
		}
	}

	const bool includesDepth = list.depthLoad.has_value();

	if (includesDepth)
	{
		graph::DepthAttachment depth;
		depth.texture = targets.depth;
		depth.loadOp = *list.depthLoad;
		depth.clearDepth = 1;
		result = builder.DepthOutput(depth);

		if (!result.IsSuccess())
		{
			return result;
		}
	}

	result = context.DeclareDrawResources(builder, list.draws, inputs);

	if (!result.IsSuccess())
	{
		return result;
	}

	// The callback owns the plan; frame and context remain borrowed for Render.
	result = builder.SetExecute(
		[&frame, list, &context](graph::RenderPassContext& pass)
		{
			const PassRecordingInputs recording = MakePassRecordingInputs(frame, list);
			return context.RecordPass(pass, recording, list.draws);
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
                                 const std::vector<PlannedPass>& lists, graph::RenderGraph& graph) const
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

	for (const PlannedPass& list : lists)
	{
		const PassRecordingInputs inputs = MakePassRecordingInputs(frame, list);

		result = context.CheckOutputReadOrder(list.draws, inputs, producers);

		if (!result.IsSuccess())
		{
			return result;
		}

		result = AddStepPass(frame, list, context, graph);

		if (!result.IsSuccess())
		{
			return result;
		}

		if (list.colorLoad.has_value())
		{
			if (const auto* target = std::get_if<RenderTargetTextureHandle>(&list.renderView.output))
			{
				producers.push_back(*target);
			}
		}
	}

	return {};
}

} // namespace rendering
