// R7 implementation guide: section 4.30.
// Select, prepare, declare and record from the same original Object indices.
// Resolve current registry revisions under single-threaded, non-reentrant Render sequencing.
// Keep Graph ownership connections here and delegate packing/PSO/draw preparation to DrawResources.

#include "FrameDrawContext.h"
#include "Failure.h"
#include "Math/Finite.h"
#include <cmath>
#include "Graph/RenderPassContext.h"

#include <algorithm>

namespace rendering::detail
{

namespace
{

// ponytail: linear deduplication keeps small Frame lists simple; index by Content if profiling warrants it.
template <class Target>
void AddUniqueUploadTarget(std::vector<Target>& targets, const Target& candidate)
{
	const auto existing = std::find_if(targets.begin(), targets.end(),
		[&candidate](const Target& target)
		{
			return target.content == candidate.content;
		});

	if (existing == targets.end())
	{
		targets.push_back(candidate);
	}
}

}

FrameDrawContext::FrameDrawContext(
	const FrameRenderData& frame,
	DrawResources& resources,
	GeometryRegistry& geometries,
	TextureRegistry& textures,
	backend::Backend& backend,
	backend::DepthTexture& depth,
	graph::FrameResourceBindings& bindings,
	std::vector<DrawDiagnostic>& diagnostics,
	std::size_t& drawnObjects,
	RenderTargetTextureRegistry* targets) noexcept
	: m_renderTargets(targets), m_frame(frame), m_resources(resources), m_geometries(geometries), m_textures(textures),
	  m_backend(backend), m_depth(depth), m_bindings(bindings),
	  m_diagnostics(diagnostics), m_drawnObjects(drawnObjects)
{}

Result FrameDrawContext::ResolveObjectInputs(
	std::size_t objectIndex,
	const DrawSettings& settings,
	DrawInputs& inputs,
	DrawDiagnostic& diagnostic)
{
	if (objectIndex >= m_frame.objects.size())
	{
		return { false, "FrameDrawContext: Object index is out of range." };
	}

	// ponytail: repeat current lookups during the mutation-free Render scope; cache only if measured costly.
	return m_resources.ResolveDrawInputs(
		m_frame.objects[objectIndex], objectIndex, settings,
		m_geometries, m_textures, inputs, diagnostic, m_renderTargets);
}

void FrameDrawContext::AddDiagnostic(
	DrawDiagnostic diagnostic,
	const std::string& stage,
	const std::string& message)
{
	diagnostic.viewId = m_activeViewId;
    const auto view = std::find_if(m_frame.views.begin(), m_frame.views.end(),
        [this](const RenderView& candidate) { return candidate.id == m_activeViewId; });
    if (view != m_frame.views.end())
    {
        diagnostic.passName = (view->kind == ViewKind::Scene ? "Scene[" : "Overlay[") + std::to_string(view->id) + "]";
    }
	diagnostic.stage = stage;
	diagnostic.message = message;
	m_diagnostics.push_back(std::move(diagnostic));
}

// Filter by explicit View membership and optional world-space Bounds before resolving resources.
// Store original Object indices only, preserving the connection between declaration and recording.
// An absent bound disables culling; an invalid bound diagnoses and skips that Object.
Result FrameDrawContext::SelectAvailableDraws(const DrawSettings& settings, RenderList& objectIndices, const RenderView* view)
{
	m_activeViewId = view ? view->id : 0;
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

        if (view && object.bounds)
        {
            const auto& bounds = *object.bounds;
            const bool validCenter = Finite(bounds.center);
            const bool validRadius = std::isfinite(bounds.radius) && bounds.radius >= 0;
            if (!validCenter || !validRadius)
            {
                DrawDiagnostic diagnostic;
                diagnostic.objectIndex = objectIndex;
                AddDiagnostic(diagnostic, "Cull", "Bounds must be a finite world sphere with nonnegative radius.");
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
                const float distance = plane.x * bounds.center.x + plane.y * bounds.center.y + plane.z * bounds.center.z + plane.w;
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
		Result result = ResolveObjectInputs(objectIndex, settings, inputs, diagnostic);

		if (!result.IsSuccess())
		{
			AddDiagnostic(diagnostic, "Resolve", result.message);
			continue;
		}

		availableObjects.push_back(objectIndex);
	}

	objectIndices = std::move(availableObjects);
	return {};
}

bool FrameDrawContext::UsesAlphaBlending(std::size_t objectIndex, const DrawSettings& settings)
{
	DrawInputs inputs;
	DrawDiagnostic diagnostic;
	Result result = ResolveObjectInputs(objectIndex, settings, inputs, diagnostic);
	return result.IsSuccess() && inputs.pipeline->desc.blend == BlendMode::Alpha;
}

// Union the ordinary Geometry/Texture revisions required by all selected Views.
// Render-target pixels are generated by attachments, so they do not enter CPU pixel Upload.
// Submit does not pin revisions: resolve the current accepted Content at this time.
Result FrameDrawContext::CollectUploadTargets(
	const RenderList& objectIndices,
	const DrawSettings& settings,
	std::vector<GeometryUploadTarget>& geometries,
	std::vector<TextureUploadTarget>& textures)
{
	for (std::size_t objectIndex : objectIndices)
	{
		if (objectIndex >= m_frame.objects.size())
		{
			return { false, "CollectUploadTargets: Object index is out of range." };
		}

		const auto& object = m_frame.objects[objectIndex];
		if (m_geometries.FindForNewUse(object.geometry))
		{
			GeometryUploadTarget target;
			target.handle = object.geometry;
			target.content = m_geometries.FindRegisteredContent(object.geometry);
			AddUniqueUploadTarget(geometries, target);
		}

		DrawInputs inputs;
		DrawDiagnostic diagnostic;
		Result result = ResolveObjectInputs(objectIndex, settings, inputs, diagnostic);

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
            result = ResolveSample(inputs.material->desc, requirement, sample);
            if (!result.IsSuccess())
            {
                return result;
            }
            if (sample.texture)
            {
                TextureUploadTarget target;
                target.handle = sample.textureHandle;
                target.content = sample.texture;
                AddUniqueUploadTarget(textures, target);
            }
		}
	}

	return {};
}

Result FrameDrawContext::ResolveSample(const MaterialDesc& material,
    const ShaderResourceBinding& requirement, SampledTexture& sample)
{
    if (m_renderTargets)
    {
        return m_resources.ResolveSampledTexture(material, requirement, m_textures, *m_renderTargets, sample);
    }
    return m_resources.ResolveTextureInput(material, requirement, m_textures, sample.textureHandle, sample.texture);
}

bool FrameDrawContext::AreResourcesPrepared(const DrawInputs& inputs)
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
        if (!ResolveSample(inputs.material->desc, requirement, sample).IsSuccess())
        {
            return false;
        }
        if (sample.texture)
        {
            const bool ready = sample.texture->state.preparationState == ResourcePreparationState::Ready;
            if (!ready || !sample.texture->gpuData)
            {
                return false;
            }
        }
        // A RenderTarget can be GPU-ready with invalid contents until its producer clears it.
        if (sample.target && !sample.target->gpuData)
        {
            return false;
        }
    }
    return true;
}

// A successful CPU Update does not imply that deferred GPU preparation succeeded.
// Skip unready Objects with diagnostics; do not silently roll them back to an older revision.
Result FrameDrawContext::CheckPreparedDraws(RenderList& objectIndices, const DrawSettings& settings)
{
	if (m_frame.views.empty())
	{
		return {};
	}

	RenderList preparedObjects;
	preparedObjects.reserve(objectIndices.size());

	for (std::size_t objectIndex : objectIndices)
	{
		DrawInputs inputs;
		DrawDiagnostic diagnostic;
		Result result = ResolveObjectInputs(objectIndex, settings, inputs, diagnostic);

		if (!result.IsSuccess())
		{
			AddDiagnostic(diagnostic, "Prepare", result.message);
			continue;
		}

		if (!AreResourcesPrepared(inputs))
		{
			AddDiagnostic(diagnostic, "Prepare", "Selected Resource preparation failed.");
			continue;
		}

		preparedObjects.push_back(objectIndex);
	}

	objectIndices = std::move(preparedObjects);
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
    if (!m_renderTargets)
    {
        return Fail("View output registry is unavailable.");
    }
    const auto handle = std::get<RenderTargetTextureHandle>(view.output);
    const auto* target = m_renderTargets->FindForNewUse(handle);
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
    m_activeViewId = view.id;
    if (const auto* window = std::get_if<WindowOutputHandle>(&view.output))
    {
        return m_bindings.PrepareViewTargets(*window, m_depth, targets, view.kind == ViewKind::Scene);
    }
    if (!m_renderTargets)
    {
        return Fail("View output registry is unavailable.");
    }
    auto* target = m_renderTargets->FindForNewUse(std::get<RenderTargetTextureHandle>(view.output));
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
	const RenderList& objectIndices,
	const DrawSettings& settings)
{
	for (std::size_t objectIndex : objectIndices)
	{
		DrawInputs inputs;
		DrawDiagnostic diagnostic;
		Result result = ResolveObjectInputs(objectIndex, settings, inputs, diagnostic);
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
            result = ResolveSample(inputs.material->desc, requirement, sample);
            if (!result.IsSuccess())
            {
                return result;
            }

            graph::GraphTextureHandle graphTexture;
            if (sample.texture)
            {
                result = m_bindings.ImportTexture(*sample.texture, requirement.name, graphTexture);
            }
            else
            {
                result = m_bindings.ImportRenderTarget(*sample.target, requirement.name, graphTexture);
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
Result FrameDrawContext::RecordRenderList(
	graph::RenderPassContext& context,
	const RenderView& view,
	const RenderList& objectIndices,
	const DrawSettings& settings)
{
    m_activeViewId = view.id;
    Result result = GetViewExtent(view, m_activeTargetWidth, m_activeTargetHeight);
    if (!result.IsSuccess())
    {
        return result;
    }
    Viewport fullOutput;
    fullOutput.width = static_cast<float>(m_activeTargetWidth);
    fullOutput.height = static_cast<float>(m_activeTargetHeight);
    m_activeViewport = view.viewport.value_or(fullOutput);
    result = m_backend.GetFrameCommandContext().SetViewport(m_activeViewport, m_activeTargetWidth, m_activeTargetHeight);
    if (!result.IsSuccess())
    {
        return result;
    }

    std::vector<DrawCommand> commands;
    result = m_resources.PrepareCommands(*this, view, objectIndices, settings, context.GetOutputLayout(), commands);
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
Result FrameDrawContext::CheckOutputReadOrder(const RenderList& indices, const DrawSettings& settings,
    const FrameRenderData& frame, const std::vector<RenderTargetTextureHandle>& producers)
{
    for (std::size_t index : indices)
    {
        DrawInputs inputs;
        DrawDiagnostic diagnostic;
        Result result = ResolveObjectInputs(index, settings, inputs, diagnostic);
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
            result = ResolveSample(inputs.material->desc, requirement, sample);
            if (!result.IsSuccess())
            {
                return result;
            }
            if (!sample.target)
            {
                continue;
            }
            const bool producedThisFrame = std::any_of(frame.views.begin(), frame.views.end(),
                [&sample](const RenderView& view)
                {
                    const auto* target = std::get_if<RenderTargetTextureHandle>(&view.output);
                    return target && *target == sample.targetHandle;
                });
            const bool alreadyProduced = std::find(producers.begin(), producers.end(), sample.targetHandle) != producers.end();
            if (producedThisFrame && !alreadyProduced)
            {
                return Fail("A current-frame RenderTarget is sampled before its producer.");
            }
        }
    }
    return {};
}

// Compute a conservative upper bound before recording: aligned constants, SRV slots and COM holds.
// Repeated common constants may make actual use smaller, but cannot make it larger than this bound.
// Reject hard-limit overflow before casts or GPU addresses/handles are issued.
Result FrameDrawContext::EstimateRequirements(const RenderList& indices, const DrawSettings& settings,
    std::uint64_t& constantBytes, std::uint64_t& descriptors, std::uint64_t& retained)
{
    for (std::size_t index : indices)
    {
        DrawInputs inputs;
        DrawDiagnostic diagnostic;
        Result result = ResolveObjectInputs(index, settings, inputs, diagnostic);
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
                retained += 2;
            }
        }
        retained += 4;
        const bool constantsExceeded = constantBytes > 64ull * 1024 * 1024;
        const bool descriptorsExceeded = descriptors > 65536;
        const bool retainedExceeded = retained > 262144;
        if (constantsExceeded || descriptorsExceeded || retainedExceeded)
        {
            return Fail("Frame requirements exceed the configured R7 hard limits.");
        }
    }
    return {};
}

}
