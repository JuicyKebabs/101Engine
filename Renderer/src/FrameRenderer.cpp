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
Result FrameRenderer::SelectViewDraws(
	const FrameRenderData& frame, const PassRecordingInputs& pass,
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

Result ReserveInternalId(const FrameRenderData& frame, const std::vector<PlannedPass>& lists, ViewId& output)
{
	ViewId candidate = (std::numeric_limits<ViewId>::max)();

	for (;;)
	{
		const bool ordinaryUse = std::any_of(
			frame.views.begin(), frame.views.end(),
			[candidate](const RenderView& view)
			{
				return view.id == candidate;
			});

		const bool auxiliaryUse = std::any_of(
			frame.auxiliaryPasses.begin(), frame.auxiliaryPasses.end(),
			[candidate](const AuxiliaryPass& pass)
			{
				return pass.id == candidate;
			});

		const bool internalUse = std::any_of(
			lists.begin(), lists.end(),
			[candidate](const PlannedPass& list)
			{
				return list.renderView.id == candidate;
			});

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

Result AppendAuxiliaryPasses(
	FrameRenderData& frame, detail::FrameDrawContext& context, std::size_t viewIndex,
	detail::SceneOutputImages* images, const ShadowInputs& shadow, std::vector<PlannedPass>& lists)
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

} // namespace

Result FrameRenderer::BuildPassPlan(
	FrameRenderData& frame,
	detail::FrameDrawContext& context,
	std::vector<PlannedPass>& plannedPasses) const
{
	// ========================================================================
	// Plan each Scene View in this order:
	// Shadow -> SceneHDR -> Auxiliary -> DisplayTransform.
	// ========================================================================

	for (std::size_t viewIndex = 0; viewIndex < frame.views.size(); ++viewIndex)
	{
		const RenderView& sourceView = frame.views[viewIndex];
		const bool isSceneView = sourceView.kind == ViewKind::Scene;

		if (!isSceneView)
		{
			continue;
		}

		// --------------------------------------------------------------------
		// 1. Prepare the images required to render the Scene.
		// --------------------------------------------------------------------

		detail::SceneOutputImages* sceneImages = nullptr;
		Result shadowPreparationResult;

		const Result scenePreparationResult = context.PrepareSceneImages(
			sourceView,
			sceneImages,
			shadowPreparationResult);

		if (!scenePreparationResult.IsSuccess())
		{
			return scenePreparationResult;
		}

		// Use disabled shadows unless a Shadow pass can be planned.
		// Update the texture and matrix after planning a valid Shadow pass.
		ShadowInputs sceneShadowInputs;
		sceneShadowInputs.texture = &context.DisabledShadow();

		const bool hasDirectionalLight = frame.directionalLight.has_value();
		const bool isShadowPreparationSuccessful =
			shadowPreparationResult.IsSuccess();
		const bool hasShadowDepthTexture =
			sceneImages->shadowDepth != nullptr;

		const bool canPlanShadowPass =
			hasDirectionalLight &&
			isShadowPreparationSuccessful &&
			hasShadowDepthTexture;

		// --------------------------------------------------------------------
		// 2. Plan the Shadow pass.
		// --------------------------------------------------------------------

		if (canPlanShadowPass)
		{
			PlannedPass shadowPass;
			shadowPass.sourceViewIndex = viewIndex;
			shadowPass.name =
				"Shadow[" + std::to_string(sourceView.id) + "]";

			// Derive a View from the light using the source Camera View.
			shadowPass.renderView = MakeShadowView(
				sourceView,
				*frame.directionalLight,
				sceneImages->hdrColor);

			shadowPass.renderView.drawSettings.pipelineOverride =
				context.ShadowPipeline();
			shadowPass.renderView.drawSettings.materialOverride.reset();

			// Clear and use only the Shadow Depth attachment, with no Color output.
			shadowPass.depthLoad = graph::AttachmentLoadOp::Clear;
			shadowPass.depth = sceneImages->shadowDepth.get();
			shadowPass.shadow.texture = &context.DisabledShadow();

			const auto shadowDepthDescription =
				sceneImages->shadowDepth->Resource()->GetDesc();

			Extent2D shadowExtent;
			shadowExtent.width =
				static_cast<std::uint32_t>(shadowDepthDescription.Width);
			shadowExtent.height = shadowDepthDescription.Height;

			shadowPass.extentOverride = shadowExtent;

			// Select using the light View rather than reusing Camera visibility.
			const PassRecordingInputs shadowRecordingInputs =
				MakePassRecordingInputs(frame, shadowPass);

			detail::RenderList shadowObjectIndices;

			const Result shadowSelectionResult = context.SelectAvailableDraws(
				shadowPass.renderView.drawSettings,
				shadowObjectIndices,
				shadowRecordingInputs);

			if (!shadowSelectionResult.IsSuccess())
			{
				return shadowSelectionResult;
			}

			// Include only selected Objects that cast shadows.

			for (std::size_t objectIndex : shadowObjectIndices)
			{
				const bool castsShadow = frame.objects[objectIndex].castShadow;

				if (!castsShadow)
				{
					continue;
				}

				PlannedDraw shadowDraw;
				shadowDraw.objectIndex = objectIndex;
				shadowDraw.settings = shadowPass.renderView.drawSettings;

				shadowPass.draws.push_back(shadowDraw);
			}

			// Finalize Shadow inputs for the following SceneHDR and Auxiliary passes.
			sceneShadowInputs.texture = sceneImages->shadowDepth.get();
			sceneShadowInputs.viewProjection =
				shadowPass.renderView.view *
				shadowPass.renderView.projection;
			sceneShadowInputs.enabled = true;

			plannedPasses.push_back(std::move(shadowPass));
		}

		// A Shadow preparation failure is diagnostic; Scene rendering continues.

		if (!isShadowPreparationSuccessful)
		{
			context.ReportViewFailure(
				sourceView.id,
				"ShadowPrepare",
				shadowPreparationResult.message);
		}

		// --------------------------------------------------------------------
		// 3. Plan the SceneHDR pass.
		// --------------------------------------------------------------------

		PlannedPass scenePass;
		scenePass.sourceViewIndex = viewIndex;
		scenePass.name =
			"SceneHDR[" + std::to_string(sourceView.id) + "]";

		// Preserve the source Camera and draw settings, but render Color to HDR.
		scenePass.renderView = sourceView;
		scenePass.renderView.output = sceneImages->hdrColor;

		scenePass.colorLoad = graph::AttachmentLoadOp::Clear;
		scenePass.depthLoad = graph::AttachmentLoadOp::Clear;
		scenePass.shadow = sceneShadowInputs;

		const PassRecordingInputs sceneRecordingInputs =
			MakePassRecordingInputs(frame, scenePass);

		detail::RenderList sceneObjectIndices;

		// Place opaque Objects first, followed by transparent Objects back to front.
		const Result sceneSelectionResult = SelectViewDraws(
			frame,
			sceneRecordingInputs,
			context,
			sceneObjectIndices);

		if (!sceneSelectionResult.IsSuccess())
		{
			return sceneSelectionResult;
		}

		for (std::size_t objectIndex : sceneObjectIndices)
		{
			PlannedDraw sceneDraw;
			sceneDraw.objectIndex = objectIndex;
			sceneDraw.settings = scenePass.renderView.drawSettings;

			scenePass.draws.push_back(sceneDraw);
		}

		plannedPasses.push_back(std::move(scenePass));

		// --------------------------------------------------------------------
		// 4. Plan Auxiliary passes associated with this Scene View.
		// --------------------------------------------------------------------

		const Result sceneAuxiliaryResult = AppendAuxiliaryPasses(
			frame,
			context,
			viewIndex,
			sceneImages,
			sceneShadowInputs,
			plannedPasses);

		if (!sceneAuxiliaryResult.IsSuccess())
		{
			return sceneAuxiliaryResult;
		}

		// --------------------------------------------------------------------
		// 5. Plan the DisplayTransform pass.
		// --------------------------------------------------------------------

		PlannedPass displayPass;
		displayPass.sourceViewIndex = viewIndex;
		displayPass.name =
			"DisplayTransform[" + std::to_string(sourceView.id) + "]";

		// Render to the output specified by the source View.
		displayPass.renderView = sourceView;
		displayPass.renderView.kind = ViewKind::Overlay;
		displayPass.colorLoad = graph::AttachmentLoadOp::Clear;
		displayPass.shadow.texture = &context.DisabledShadow();

		// Convert the entire HDR image without inheriting a partial Viewport.
		displayPass.renderView.viewport.reset();

		DrawSettings displayDrawSettings;
		displayPass.renderView.drawSettings = displayDrawSettings;

		// Use a private ID so the internal Object cannot enter normal Scene selection.
		const Result displayIdReservationResult = ReserveInternalId(
			frame,
			plannedPasses,
			displayPass.renderView.id);

		if (!displayIdReservationResult.IsSuccess())
		{
			return displayIdReservationResult;
		}

		// Draw a fullscreen triangle using the Display Material.
		ProceduralGeometry fullscreenTriangle;
		fullscreenTriangle.vertexCount = 3;

		RenderObject displayObject;
		displayObject.material = sceneImages->displayMaterial;
		displayObject.procedural = fullscreenTriangle;
		displayObject.targetViews.push_back(displayPass.renderView.id);

		const std::size_t displayObjectIndex = frame.objects.size();
		frame.objects.push_back(std::move(displayObject));

		PlannedDraw displayDraw;
		displayDraw.objectIndex = displayObjectIndex;
		displayDraw.settings = displayPass.renderView.drawSettings;

		displayPass.draws.push_back(displayDraw);

		plannedPasses.push_back(std::move(displayPass));
	}

	// ========================================================================
	// After all Scene Views, plan each Overlay View in this order:
	// Overlay -> Auxiliary.
	// ========================================================================

	for (std::size_t viewIndex = 0; viewIndex < frame.views.size(); ++viewIndex)
	{
		const RenderView& sourceView = frame.views[viewIndex];
		const bool isOverlayView = sourceView.kind == ViewKind::Overlay;

		if (!isOverlayView)
		{
			continue;
		}

		// --------------------------------------------------------------------
		// 6. Plan the Overlay pass.
		// --------------------------------------------------------------------

		PlannedPass overlayPass;
		overlayPass.sourceViewIndex = viewIndex;
		overlayPass.name =
			"Overlay[" + std::to_string(sourceView.id) + "]";

		overlayPass.renderView = sourceView;
		overlayPass.renderView.drawSettings.depthOverride =
			DepthMode::Disabled;

		overlayPass.shadow.texture = &context.DisabledShadow();

		// Preserve existing contents if a previous pass writes to this output.
		bool hasPreviousColorOutput = false;

		for (const PlannedPass& previousPass : plannedPasses)
		{
			const bool hasColorAttachment =
				previousPass.colorLoad.has_value();

			if (!hasColorAttachment)
			{
				continue;
			}

			const bool writesToSameOutput = SameViewOutput(
				previousPass.renderView.output,
				sourceView.output);

			if (writesToSameOutput)
			{
				hasPreviousColorOutput = true;
				break;
			}

		}

		overlayPass.colorLoad = graph::AttachmentLoadOp::Clear;

		if (hasPreviousColorOutput)
		{
			overlayPass.colorLoad = graph::AttachmentLoadOp::Load;
		}

		const PassRecordingInputs overlayRecordingInputs =
			MakePassRecordingInputs(frame, overlayPass);

		detail::RenderList overlayObjectIndices;

		// SelectViewDraws preserves input order without sorting Overlay Objects.
		const Result overlaySelectionResult = SelectViewDraws(
			frame,
			overlayRecordingInputs,
			context,
			overlayObjectIndices);

		if (!overlaySelectionResult.IsSuccess())
		{
			return overlaySelectionResult;
		}

		for (std::size_t objectIndex : overlayObjectIndices)
		{
			PlannedDraw overlayDraw;
			overlayDraw.objectIndex = objectIndex;
			overlayDraw.settings = overlayPass.renderView.drawSettings;

			overlayPass.draws.push_back(overlayDraw);
		}

		const ShadowInputs overlayShadowInputs = overlayPass.shadow;

		plannedPasses.push_back(std::move(overlayPass));

		// --------------------------------------------------------------------
		// 7. Plan Auxiliary passes associated with this Overlay View.
		// --------------------------------------------------------------------

		detail::SceneOutputImages* overlaySceneImages = nullptr;

		const Result overlayAuxiliaryResult = AppendAuxiliaryPasses(
			frame,
			context,
			viewIndex,
			overlaySceneImages,
			overlayShadowInputs,
			plannedPasses);

		if (!overlayAuxiliaryResult.IsSuccess())
		{
			return overlayAuxiliaryResult;
		}

	}

	return {};
}

Result FrameRenderer::AddStepPass(
	const FrameRenderData& frame,
	const PlannedPass& list,
	detail::FrameDrawContext& context,
	graph::RenderGraph& graph) const
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

Result FrameRenderer::BuildRenderPasses(
	const FrameRenderData& frame,
	detail::FrameDrawContext& context,
	const std::vector<PlannedPass>& lists,
	graph::RenderGraph& graph) const
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
