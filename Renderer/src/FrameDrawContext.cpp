// R7 implementation guide: section 4.30.
// Select, prepare, declare and record from the same original Object indices.
// Resolve current registry revisions under single-threaded, non-reentrant Render sequencing.
// Keep Graph ownership connections here and delegate packing/PSO/draw preparation to DrawResources.

#include "FrameDrawContext.h"
#include "SceneOutputResources.h"
#include "Failure.h"
#include "Math/Finite.h"
#include <cmath>
#include <limits>
#include "Graph/RenderPassContext.h"

#include <algorithm>

namespace rendering::detail
{

namespace
{

// ponytail: linear deduplication keeps small Frame lists simple; index by Content if profiling warrants it.
template <class Target> void AddUniqueUploadTarget(std::vector<Target>& targets, const Target& candidate)
{
	const auto existing = std::find_if(targets.begin(), targets.end(), [&candidate](const Target& target)
                                       { return target.content == candidate.content; });

	if (existing == targets.end())
	{
		targets.push_back(candidate);
	}
}

} // namespace

FrameDrawContext::FrameDrawContext(
	const FrameRenderData& frame, const FrameDrawServices& services,
	std::vector<DrawDiagnostic>& diagnostics, std::size_t& drawnObjects) noexcept
	: m_sceneResources(services.sceneResources), m_frame(frame),
	  m_resources(*services.draws), m_backend(*services.backend), m_depth(*services.depth),
	  m_bindings(*services.bindings), m_diagnostics(diagnostics), m_drawnObjects(drawnObjects)
{
}

Result FrameDrawContext::PrepareSceneImages(const RenderView& view, SceneOutputImages*& images, Result& shadowPreparation)
{
	if (!m_sceneResources)
	{
		return Fail("Scene working-image owner is unavailable.");
	}

	return m_sceneResources->Prepare(
		view, m_backend, m_resources, images,
		shadowPreparation, m_frame.directionalLight.has_value());
}

backend::DepthTexture& FrameDrawContext::DisabledShadow() noexcept
{
	return m_sceneResources->DisabledShadow();
}

backend::DepthTexture* FrameDrawContext::SceneDepth(SceneOutputImages& images) noexcept
{
	auto* hdr = m_resources.FindContent(images.hdrColor);
	return hdr ? &hdr->gpuData->Depth() : nullptr;
}

void FrameDrawContext::ReportViewFailure(ViewId viewId, const std::string& stage, const std::string& message)
{
	PassRecordingInputs pass;
	pass.viewId = viewId;
	pass.passName = "View[" + std::to_string(viewId) + "]";
	DrawDiagnostic diagnostic;
	diagnostic.objectIndex = (std::numeric_limits<std::size_t>::max)();
	AddDiagnostic(diagnostic, stage, message, pass);
}

PipelineHandle FrameDrawContext::ShadowPipeline() const noexcept
{
	return m_sceneResources->ShadowPipeline();
}

bool FrameDrawContext::SelectExplicitDraw(std::size_t index, const DrawSettings& settings, const PassRecordingInputs& pass)
{
	DrawInputs inputs;
	DrawDiagnostic diagnostic;
	Result result = ResolveObjectInputs(index, settings, inputs, diagnostic, pass);

	if (!result.IsSuccess())
	{
		AddDiagnostic(diagnostic, "AuxiliaryResolve", result.message, pass);
		return false;
	}

	return true;
}

Result FrameDrawContext::ResolveObjectInputs(
	std::size_t objectIndex, const DrawSettings& settings, DrawInputs& inputs,
	DrawDiagnostic& diagnostic, const PassRecordingInputs& pass)
{
	if (objectIndex >= m_frame.objects.size())
	{
		return {false, "FrameDrawContext: Object index is out of range."};
	}

	const RenderObject& sourceObject = m_frame.objects[objectIndex];
	Result resolved = m_resources.ResolveDrawInputs(sourceObject, objectIndex, settings, inputs, diagnostic);

	if (resolved.IsSuccess())
	{
		for (const auto& requirement : inputs.pipeline->layout.resources)
		{
			if (requirement.sampler || requirement.name == "RendererShadowMap")
			{
				continue;
			}

			const MaterialParameter* parameter = FindMaterialParameter(inputs.material->desc, requirement.name);

			if (!parameter)
			{
				continue;
			}

			// Check if the parameter is a TextureHandle or RenderTargetTextureHandle and validate its availability
			if (const auto* texture = std::get_if<TextureHandle>(&parameter->value))
			{
				if (!m_resources.FindContent(*texture))
				{
					AddDiagnostic(diagnostic, "TextureFallback",
								requirement.name + ": " + m_resources.DescribeTextureUnavailable(*texture) +
								"; resident error Texture substituted.", pass);
				}
			}
			else if (const auto* target = std::get_if<RenderTargetTextureHandle>(&parameter->value))
			{
				if (!m_resources.FindContent(*target))
				{
					AddDiagnostic(diagnostic, "TextureFallback",
								requirement.name + ": " + m_resources.DescribeRenderTargetTextureUnavailable(*target) +
								"; resident error Texture substituted.", pass);
				}
			}
		}

		return resolved;
	}

	// Keep the first diagnostic and original public handles. This local copy is
	// used for one resolution only; it is not a second persistent Object array.
	const std::string originalFailure = resolved.message;
	DrawDiagnostic sourceDiagnostic = diagnostic;
	RenderObject effectiveObject = sourceObject;
	DrawSettings effectiveSettings = settings;

	const bool geometryUnavailable = !sourceObject.procedural && !m_resources.FindContent(sourceObject.geometry);

	if (geometryUnavailable)
	{
		effectiveObject.geometry = m_resources.fallbackGeometry;
		effectiveObject.bounds.reset();

		DrawRange completeFallbackRange;
		effectiveObject.drawRange = completeFallbackRange;

		Vector4 neutralTint;
		neutralTint.x = 1;
		neutralTint.y = 1;
		neutralTint.z = 1;
		neutralTint.w = 1;
		effectiveObject.tint = neutralTint;
	}

	effectiveObject.material = m_resources.fallbackMaterial;
	effectiveSettings.materialOverride.reset();

	if (effectiveSettings.pipelineOverride)
	{
		const auto* overridePipeline = m_resources.FindContent(*effectiveSettings.pipelineOverride);
		const bool compatibleDepthOnly = overridePipeline && overridePipeline->desc.pixelShader == ShaderHandle{};

		if (!compatibleDepthOnly)
		{
			effectiveSettings.pipelineOverride.reset();
		}
	}

	resolved = m_resources.ResolveDrawInputs(effectiveObject, objectIndex, effectiveSettings, inputs, diagnostic);

	if (!resolved.IsSuccess())
	{
		diagnostic = sourceDiagnostic;
		return Fail(originalFailure + " Fallback is incompatible: " + resolved.message);
	}

	inputs.geometrySubstituted = geometryUnavailable;
	AddDiagnostic(sourceDiagnostic, "Fallback",
                  originalFailure + (geometryUnavailable ? " Using resident Geometry and Material."
                                                         : " Using resident Material on the original Geometry."),
                  pass);
	return {};
}

void FrameDrawContext::AddDiagnostic(DrawDiagnostic diagnostic, const std::string& stage, const std::string& message,
                                     const PassRecordingInputs& pass)
{
	// One root cause may be observed during select, estimate and record. Retain
	// one message for that Object/View without hiding different failure causes.
	const bool alreadyReported =
		std::any_of(m_diagnostics.begin(), m_diagnostics.end(),
					[&](const DrawDiagnostic& previous)
					{
						const bool sameLocation =
							previous.objectIndex == diagnostic.objectIndex && previous.viewId == pass.viewId;
						const bool sameCause = previous.stage == stage && previous.message == message;
						return sameLocation && sameCause;
					});

	if (alreadyReported)
	{
		return;
	}

	diagnostic.viewId = pass.viewId;
	diagnostic.passName = pass.passName;

	diagnostic.stage = stage;
	diagnostic.message = message;
	m_diagnostics.push_back(std::move(diagnostic));
}

// Filter by explicit View membership and optional world-space Bounds before resolving resources.
// Store original Object indices only, preserving the connection between declaration and recording.
// An absent bound disables culling; an invalid bound diagnoses and skips that Object.
Result FrameDrawContext::SelectAvailableDraws(const DrawSettings& settings, RenderList& objectIndices,
                                              const PassRecordingInputs& pass)
{
	const RenderView* view = pass.view;
	RenderList availableObjects;
	availableObjects.reserve(m_frame.objects.size());

	for (std::size_t objectIndex = 0; objectIndex < m_frame.objects.size(); ++objectIndex)
	{
		// A View-less submission may still prepare Geometry without a Material.
		if (m_frame.views.empty())
		{
			availableObjects.push_back(objectIndex);
			continue;
		}

		const auto& object = m_frame.objects[objectIndex];

		if (view && !object.targetViews.empty())
		{
			if (std::find(object.targetViews.begin(), object.targetViews.end(), view->id) == object.targetViews.end())
			{
				continue;
			}
		}

		// Resolve before culling: fallback selection must not be hidden by the
		// original Geometry's obsolete Bounds. A submitted revision mismatch
		// intentionally performs extra work instead of losing visible geometry.
		const GeometryContent* boundsGeometry = m_resources.FindContent(object.geometry);
		const bool proceduralBounds = object.procedural.has_value();
		const bool currentGeometryBounds = boundsGeometry && object.boundsGeometryRevision != 0 &&
                                           boundsGeometry->revision == object.boundsGeometryRevision;
		const bool mayCull = proceduralBounds || currentGeometryBounds;

		if (view && object.bounds && mayCull)
		{
			const auto& bounds = *object.bounds;
			const bool validCenter = Finite(bounds.center);
			const bool validRadius = std::isfinite(bounds.radius) && bounds.radius >= 0;

			if (!validCenter || !validRadius)
			{
				DrawDiagnostic diagnostic;
				diagnostic.objectIndex = objectIndex;
				AddDiagnostic(diagnostic, "Cull", "Bounds must be a finite world sphere with nonnegative radius.",
                              pass);
				continue;
			}

			// With row vectors, world points multiply View * Projection on the right.
			// Left/right and top/bottom planes use column3 +/- column0/1.
			// DX12 near uses z >= 0 (column2), unlike a symmetric -w <= z convention; far uses column3-column2.
			// Compare signed distance against -radius * normal length, avoiding plane normalization.
			const Matrix4x4 transform = view->view * view->projection;
			const auto& matrix = transform.m;

			bool outside = false;

			for (int planeIndex = 0; planeIndex < 6; ++planeIndex)
			{
				const int column = planeIndex / 2;
				const float sign = planeIndex % 2 == 0 ? 1.0f : -1.0f;

				std::array<float, 4> coefficients{};

				for (int row = 0; row < 4; ++row)
				{
					float coefficient = matrix[row][3] + sign * matrix[row][column];

					if (planeIndex == 4)
					{
						coefficient = matrix[row][2];
					}

					coefficients[row] = coefficient;
				}

				Vector4 plane;
				plane.x = coefficients[0];
				plane.y = coefficients[1];
				plane.z = coefficients[2];
				plane.w = coefficients[3];

				const float length = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
				const float distance =
					plane.x * bounds.center.x + plane.y * bounds.center.y + plane.z * bounds.center.z + plane.w;

				if (distance < -bounds.radius * length)
				{
					outside = true;
					break;
				}
			}

			if (outside)
			{
				continue;
			}
		}

		DrawInputs inputs;
		DrawDiagnostic diagnostic;
		Result result = ResolveObjectInputs(objectIndex, settings, inputs, diagnostic, pass);

		if (!result.IsSuccess())
		{
			AddDiagnostic(diagnostic, "Resolve", result.message, pass);
			continue;
		}

		availableObjects.push_back(objectIndex);
	}

	objectIndices = std::move(availableObjects);
	return {};
}

bool FrameDrawContext::UsesAlphaBlending(std::size_t objectIndex, const DrawSettings& settings,
                                         const PassRecordingInputs& pass)
{
	DrawInputs inputs;
	DrawDiagnostic diagnostic;
	Result result = ResolveObjectInputs(objectIndex, settings, inputs, diagnostic, pass);
	return result.IsSuccess() && inputs.pipeline->desc.blend == BlendMode::Alpha;
}

// Union the ordinary Geometry/Texture revisions required by all selected Views.
// Render-target pixels are generated by attachments, so they do not enter CPU pixel Upload.
// Submit does not pin revisions: resolve the current accepted Content at this time.
Result FrameDrawContext::CollectUploadTargets(std::span<const PlannedDraw> draws, const PassRecordingInputs& pass,
                                              std::vector<GeometryUploadTarget>& geometries,
                                              std::vector<TextureUploadTarget>& textures)
{
	for (const PlannedDraw& draw : draws)
	{
		const std::size_t objectIndex = draw.objectIndex;
		const DrawSettings& settings = draw.settings;
		if (objectIndex >= m_frame.objects.size())
		{
			return {false, "CollectUploadTargets: Object index is out of range."};
		}

		const auto& object = m_frame.objects[objectIndex];

		if (m_resources.FindContent(object.geometry))
		{
			GeometryUploadTarget target;
			target.handle = object.geometry;
			target.content = m_resources.FindContent(object.geometry);
			AddUniqueUploadTarget(geometries, target);
		}

		DrawInputs inputs;
		DrawDiagnostic diagnostic;
		Result result = ResolveObjectInputs(objectIndex, settings, inputs, diagnostic, pass);

		if (!result.IsSuccess())
		{
			if (m_frame.views.empty())
			{
				continue;
			}

			return result;
		}

		for (const auto& requirement : inputs.pipeline->layout.resources)
		{
			if (requirement.sampler)
			{
				continue;
			}

			SampledTexture sample;
			result = ResolveSample(inputs.material->desc, requirement, sample, pass);

			if (!result.IsSuccess())
			{
				return result;
			}

			if (const auto* texture = std::get_if<UploadedTextureInput>(&sample))
			{
				TextureUploadTarget target;
				target.handle = texture->handle;
				target.content = texture->content;
				AddUniqueUploadTarget(textures, target);
			}
		}
	}

	return {};
}

Result FrameDrawContext::ResolveSample(
	const MaterialDesc& material,
	const ShaderResourceBinding& requirement,
	SampledTexture& sample,
	const PassRecordingInputs& pass)
{
	if (requirement.name == "RendererShadowMap")
	{
		if (!pass.shadow.texture || !pass.shadow.texture->AllowsSampling())
		{
			return Fail("The Scene View has no safe common Shadow sample binding.");
		}

		sample = pass.shadow.texture;
		return {};
	}

	return m_resources.ResolveSampledTexture(material, requirement, sample);
}

bool FrameDrawContext::AreResourcesPrepared(const DrawInputs& inputs, const PassRecordingInputs& pass)
{
	if (inputs.geometry)
	{
		const bool ready = inputs.geometry->state.preparationState == ResourcePreparationState::Ready;

		if (!ready || !inputs.geometry->gpuData)
		{
			return false;
		}
	}

	for (const auto& requirement : inputs.pipeline->layout.resources)
	{
		if (requirement.sampler)
		{
			continue;
		}

		SampledTexture sample;

		if (!ResolveSample(inputs.material->desc, requirement, sample, pass).IsSuccess())
		{
			return false;
		}

		if (const auto* texture = std::get_if<UploadedTextureInput>(&sample))
		{
			const bool ready = texture->content->state.preparationState == ResourcePreparationState::Ready;

			if (!ready || !texture->content->gpuData)
			{
				return false;
			}
		}

		// A RenderTarget can be GPU-ready with invalid contents until its producer clears it.
		const auto* target = std::get_if<RenderTargetInput>(&sample);

		if (target && !target->content->gpuData)
		{
			return false;
		}
	}

	return true;
}

// A successful CPU Update does not imply that deferred GPU preparation succeeded.
// Skip unready Objects with diagnostics; do not silently roll them back to an older revision.
Result FrameDrawContext::CheckPreparedDraws(std::vector<PlannedDraw>& draws, const PassRecordingInputs& pass)
{
	if (m_frame.views.empty())
	{
		return {};
	}

	std::vector<PlannedDraw> preparedObjects;
	preparedObjects.reserve(draws.size());

	for (const PlannedDraw& draw : draws)
	{
		const std::size_t objectIndex = draw.objectIndex;
		const DrawSettings& settings = draw.settings;
		DrawInputs inputs;
		DrawDiagnostic diagnostic;
		Result result = ResolveObjectInputs(objectIndex, settings, inputs, diagnostic, pass);

		if (!result.IsSuccess())
		{
			AddDiagnostic(diagnostic, "Prepare", result.message, pass);
			continue;
		}

		if (!AreResourcesPrepared(inputs, pass))
		{
			AddDiagnostic(diagnostic, "Prepare", "Selected Resource preparation failed.", pass);
			continue;
		}

		preparedObjects.push_back(draw);
	}

	draws = std::move(preparedObjects);
	return {};
}

// Query the current logical output owner, so Submit-before-Resize sees the replacement extent.
// Only omitted viewport dimensions follow this automatically; the submitted Camera projection stays unchanged.
Result FrameDrawContext::GetViewExtent(const RenderView& view, std::uint32_t& width, std::uint32_t& height)
{
	if (const auto* window = std::get_if<WindowOutputHandle>(&view.output))
	{
		return m_backend.GetWindowOutputSize(*window, width, height);
	}

	const auto handle = std::get<RenderTargetTextureHandle>(view.output);
	const auto* target = m_resources.FindContent(handle);

	if (!target || !target->gpuData)
	{
		return Fail("View output is unavailable.");
	}

	width = target->gpuData->width;
	height = target->gpuData->height;
	return {};
}

// Delegate owner-to-Graph connection to FrameResourceBindings.
// Scene requests Depth; Overlay does not. Color and Depth replacement must have finished before Import.
Result FrameDrawContext::PrepareViewTargets(const RenderView& view, graph::ViewTargets& targets)
{

	if (const auto* window = std::get_if<WindowOutputHandle>(&view.output))
	{
		return m_bindings.PrepareViewTargets(*window, m_depth, targets, view.kind == ViewKind::Scene);
	}

	auto* target = m_resources.FindContent(std::get<RenderTargetTextureHandle>(view.output));

	if (!target)
	{
		return Fail("View output was destroyed or is invalid.");
	}

	return m_bindings.PrepareRenderTargetTargets(*target, view.kind == ViewKind::Scene, targets);
}

// Declare exact sampled resources required by the final Pipeline before Graph Compile.
// Normal textures and render-target samples use separate owners but converge to a Graph ShaderRead usage.
Result FrameDrawContext::DeclareDrawResources(
	graph::RenderPassBuilder& builder,
	std::span<const PlannedDraw> draws,
	const PassRecordingInputs& pass)
{
	for (const PlannedDraw& draw : draws)
	{
		const std::size_t objectIndex = draw.objectIndex;
		const DrawSettings& settings = draw.settings;
		DrawInputs inputs;
		DrawDiagnostic diagnostic;
		Result result = ResolveObjectInputs(objectIndex, settings, inputs, diagnostic, pass);

		if (!result.IsSuccess())
		{
			return result;
		}

		for (const auto& requirement : inputs.pipeline->layout.resources)
		{
			if (requirement.sampler)
			{
				continue;
			}

			SampledTexture sample;
			result = ResolveSample(inputs.material->desc, requirement, sample, pass);

			if (!result.IsSuccess())
			{
				return result;
			}

			graph::GraphTextureHandle graphTexture;

			if (const auto* texture = std::get_if<UploadedTextureInput>(&sample))
			{
				result = m_bindings.ImportTexture(*texture->content, requirement.name, graphTexture);
			}
			else if (const auto* target = std::get_if<RenderTargetInput>(&sample))
			{
				result = m_bindings.ImportRenderTarget(*target->content, requirement.name, graphTexture);
			}
			else
			{
				result = m_bindings.ImportDepth(*std::get<backend::DepthTexture*>(sample), graphTexture);
			}

			if (!result.IsSuccess())
			{
				return result;
			}

			const auto shaderStages = static_cast<graph::ShaderStage>(static_cast<unsigned>(requirement.stages));
			result = builder.ReadTexture(graphTexture, shaderStages);

			if (!result.IsSuccess())
			{
				return result;
			}
		}
	}

	return {};
}

// Set the View context and viewport before preparing this View's DrawCommands.
// The same source indices and settings used in declaration are used here.
// DrawResources owns packing/PSO preparation; this borrowed gateway does not duplicate them.
Result FrameDrawContext::RecordPass(
	graph::RenderPassContext& context,
	const PassRecordingInputs& pass,
	std::span<const PlannedDraw> draws)
{
	if (draws.empty())
	{
		return {};
	}

	PassRecordingInputs recording = pass;
	Result result = GetViewExtent(*pass.view, recording.extent.width, recording.extent.height);

	if (!result.IsSuccess())
	{
		return result;
	}

	if (pass.extentOverride)
	{
		recording.extent = *pass.extentOverride;
	}

	Viewport fullOutput;
	fullOutput.width = static_cast<float>(recording.extent.width);
	fullOutput.height = static_cast<float>(recording.extent.height);
	recording.viewport = pass.view->viewport.value_or(fullOutput);
	result = m_backend.GetFrameCommandContext().SetViewport(recording.viewport, recording.extent.width,
															recording.extent.height);

	if (!result.IsSuccess())
	{
		return result;
	}

	std::vector<DrawCommand> commands;
	result = m_resources.PrepareCommands(*this, recording, draws, context.GetOutputLayout(), commands);

	if (!result.IsSuccess())
	{
		return result;
	}

	result = context.RecordDrawCommands(commands);

	if (result.IsSuccess())
	{
		m_drawnObjects += commands.size();
	}

	return result;
}

// If an output is produced in this frame, require its producer to precede this read.
// Last frame's valid pixels do not excuse reading a future current-frame producer.
// An image not produced this frame can reuse its last valid contents, subject to Graph validation.
Result FrameDrawContext::CheckOutputReadOrder(
		std::span<const PlannedDraw> draws,
		const PassRecordingInputs& pass,
		const std::vector<RenderTargetTextureHandle>& producers)
{
	for (const PlannedDraw& draw : draws)
	{
		const std::size_t index = draw.objectIndex;
		const DrawSettings& settings = draw.settings;
		DrawInputs inputs;
		DrawDiagnostic diagnostic;
		Result result = ResolveObjectInputs(index, settings, inputs, diagnostic, pass);

		if (!result.IsSuccess())
		{
			return result;
		}

		for (const auto& requirement : inputs.pipeline->layout.resources)
		{
			if (requirement.sampler)
			{
				continue;
			}

			SampledTexture sample;
			result = ResolveSample(inputs.material->desc, requirement, sample, pass);

			if (!result.IsSuccess())
			{
				return result;
			}

			const auto* sampledTarget = std::get_if<RenderTargetInput>(&sample);

			if (!sampledTarget)
			{
				continue;
			}

			const bool producedThisFrame = 
				std::any_of(
					m_frame.views.begin(), m_frame.views.end(),
					[sampledTarget](const RenderView& view)
					{
						const auto* target = std::get_if<RenderTargetTextureHandle>(&view.output);
						return target && *target == sampledTarget->handle;
					});

			const bool auxiliaryProducer =
				std::any_of(
					m_frame.auxiliaryPasses.begin(), m_frame.auxiliaryPasses.end(),
					[sampledTarget](const AuxiliaryPass& pass)
					{ 
						return pass.colorOutput && *pass.colorOutput == sampledTarget->handle;
					});

			const bool alreadyProduced =
				std::find(producers.begin(), producers.end(), sampledTarget->handle) != producers.end();

			if ((producedThisFrame || auxiliaryProducer) && !alreadyProduced)
			{
				return Fail("A current-frame RenderTarget is sampled before its producer.");
			}
		}
	}

	return {};
}

// Compute a conservative upper bound before recording: aligned constants and SRV slots.
// Repeated common constants may make actual use smaller, but cannot make it larger than this bound.
// Reject hard-limit overflow before casts or GPU addresses/handles are issued.
Result FrameDrawContext::EstimateRequirements(
	std::span<const PlannedDraw> draws,
	const PassRecordingInputs& pass,
	std::uint64_t& constantBytes, std::uint64_t& descriptors)
{
	for (const PlannedDraw& draw : draws)
	{
		const std::size_t index = draw.objectIndex;
		const DrawSettings& settings = draw.settings;
		DrawInputs inputs;
		DrawDiagnostic diagnostic;
		Result result = ResolveObjectInputs(index, settings, inputs, diagnostic, pass);

		if (!result.IsSuccess())
		{
			return result;
		}

		for (const auto& buffer : inputs.pipeline->layout.constants)
		{
			constantBytes += (static_cast<std::uint64_t>(buffer.size) + 255) & ~std::uint64_t(255);
		}

		for (const auto& resource : inputs.pipeline->layout.resources)
		{
			if (!resource.sampler)
			{
				++descriptors;
			}
		}

		const bool constantsExceeded = constantBytes > 64ull * 1024 * 1024;
		const bool descriptorsExceeded = descriptors > 65536;

		if (constantsExceeded || descriptorsExceeded)
		{
			return Fail("Frame requirements exceed the configured R7 hard limits.");
		}
	}

	return {};
}

} // namespace rendering::detail
