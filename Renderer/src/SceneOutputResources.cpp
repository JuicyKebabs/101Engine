#include "SceneOutputResources.h"
#include "BuiltinShaderSources.h"
#include "FrameRenderer.h"
#include "Failure.h"

#include <algorithm>

namespace rendering::detail
{

Result SceneOutputResources::Initialize(backend::Backend& backend, DrawResources& draws)
{
	// A valid, clearable one-pixel depth image provides a safe binding when a
	// View has no light or its full-sized Shadow allocation cannot be prepared.
	Result result = m_disabledShadow.Ensure(backend.GetNativeDevice(), 1, 1, true);

	if (!result.IsSuccess())
	{
		return result;
	}

	//----- Built-in Shaders and Pipelines ------

	ShaderHandle fullscreenVertex;
	result = draws.RegisterBuiltinShader(BuiltinFullscreenShader, ShaderStage::Vertex, fullscreenVertex);

	if (!result.IsSuccess())
	{
		return result;
	}

	ShaderHandle displayPixel;
	result = draws.RegisterBuiltinShader(BuiltinDisplayShader, ShaderStage::Pixel, displayPixel);

	if (!result.IsSuccess())
	{
		return result;
	}

	PipelineDesc displayDescription;
	displayDescription.vertexShader = fullscreenVertex;
	displayDescription.pixelShader = displayPixel;
	displayDescription.depth = DepthMode::Disabled;
	displayDescription.cull = CullMode::None;
	ResourceRegistrationResult<PipelineHandle> displayRegistration = draws.Register(displayDescription);

	if (!displayRegistration.result.IsSuccess())
	{
		return displayRegistration.result;
	}

	m_displayPipeline = displayRegistration.handle;

	ShaderHandle shadowVertex;
	result = draws.RegisterBuiltinShader(BuiltinMeshShader, ShaderStage::Vertex, shadowVertex);

	if (!result.IsSuccess())
	{
		return result;
	}

	PipelineDesc shadowDescription;
	shadowDescription.vertexShader = shadowVertex;
	shadowDescription.depth = DepthMode::ReadWrite;
	shadowDescription.cull = CullMode::None;
	ResourceRegistrationResult<PipelineHandle> shadowRegistration = draws.Register(shadowDescription);

	if (!shadowRegistration.result.IsSuccess())
	{
		return shadowRegistration.result;
	}

	m_shadowPipeline = shadowRegistration.handle;
	return {};
}

namespace
{

// Get the output extent for a RenderView, either from the WindowOutput or the RenderTargetTexture.
Result QueryOutputExtent(
	const RenderView& view,
	backend::Backend& backend,
	DrawResources& draws,
	Extent2D& extent)
{
	std::uint32_t& width = extent.width;
	std::uint32_t& height = extent.height;
	Result result;

	if (const auto* window = std::get_if<WindowOutputHandle>(&view.output))
	{
		result = backend.GetWindowOutputSize(*window, width, height);

		if (!result.IsSuccess())
		{
			return result;
		}
	}
	else
	{
		const auto* output = draws.FindContent(std::get<RenderTargetTextureHandle>(view.output));

		if (!output || !output->gpuData)
		{
			return Fail("Scene output is unavailable; fallback sampling cannot replace an output attachment.");
		}

		width = output->gpuData->width;
		height = output->gpuData->height;
	}

	return {};
}

Result EnsureHdrOutput(
	const RenderView& view,
	const Extent2D& extent,
	backend::Backend& backend,
	DrawResources& draws,
	SceneOutputImages& images)
{
	// Extent is already validated at the beginning of the SceneOutputResources::Prepare.
	const std::uint32_t width = extent.width;
	const std::uint32_t height = extent.height;
	Result result;

	if (images.hdrColor == RenderTargetTextureHandle{})
	{// No cached HDR image exists; create one for this output identity.
		RenderTargetTextureDesc description;
		description.width = width;
		description.height = height;
		description.format = TextureFormat::RGBA16_Float;	// HDR format; Engine currently uses 16-bit float for HDR.
		
		// The private HDR image is normally cleared with its source View's background.
		// This is fixed at creation, independently of later Passes.
		description.optimizedClearColor = view.clearColor;	// Currently this value is not used by the Engine; it is a Renderer implementation proposal.
		
		// Create internal GPU resources for the HDR output of this view. The logical handle is cached for this output identity.
		ResourceRegistrationResult<RenderTargetTextureHandle> registration = draws.CreateRenderTargetTexture(backend.GetNativeDevice(), description);

		if (!registration.result.IsSuccess())
		{
			return registration.result;
		}

		images.hdrColor = registration.handle;
	}
	else
	{// A cached HDR image exists; check if its extent matches the current View.
		const auto* hdr = draws.FindContent(images.hdrColor);

		// Detect the resizing of the HDR output. The logical handle is preserved, so the physical GPU will be replaced.
		const bool changedExtent = hdr && (hdr->gpuData->width != width || hdr->gpuData->height != height);

		if (changedExtent)
		{
			// Replacement is exceptional, never a per-frame wait/reallocation.
			// No Graph has imported the old image during this preparation phase.
			result = backend.WaitIdle();

			if (!result.IsSuccess())
			{
				return result;
			}

			// Replacement of the GPU resources based on the logical handle.
			result = draws.ResizeRenderTargetTexture(backend.GetNativeDevice(), images.hdrColor, width, height);

			if (!result.IsSuccess())
			{
				return result;
			}
		}
	}

	return {};
}

Result EnsureDisplayMaterial(
	const RenderView& view,
	DrawResources& draws,
	PipelineHandle displayPipeline,
	SceneOutputImages& images)
{
	Result result;

	if (images.displayMaterial == MaterialHandle{} || images.exposure != view.exposure)
	{// Create or update the display Material for this output identity.
		MaterialParameter textureParameter;
		textureParameter.name = "HDRTexture";
		textureParameter.value = images.hdrColor;
		MaterialParameter exposureParameter;
		exposureParameter.name = "Exposure";
		exposureParameter.value = view.exposure;

		MaterialDesc description;
		description.pipeline = displayPipeline;
		description.parameters.push_back(textureParameter);
		description.parameters.push_back(exposureParameter);

		if (images.displayMaterial == MaterialHandle{})
		{// Register a new Material for the display pass if first time or after a previous Destroy.
			ResourceRegistrationResult<MaterialHandle> registration = draws.Register(description);

			if (!registration.result.IsSuccess())
			{
				return registration.result;
			}

			images.displayMaterial = registration.handle;
		}
		else
		{// Update the existing Material for the display pass if the exposure has changed.
			result = draws.UpdateMaterial(images.displayMaterial, description);

			if (!result.IsSuccess())
			{
				return result;
			}
		}

		images.exposure = view.exposure;
	}

	return {};
}

Result EnsureShadowDepth(backend::Backend& backend, SceneOutputImages& images, Result& shadowPreparation)
{
	if (!images.shadowDepth)
	{// No cached Shadow depth image exists; create one for this output identity.
		auto candidate = std::make_unique<backend::DepthTexture>();

		// Fixed 4096x4096 size is a proposal for the Engine's shadow map.
		shadowPreparation = candidate->Ensure(backend.GetNativeDevice(), 4096, 4096, true);

		if (shadowPreparation.IsSuccess())
		{
			images.shadowDepth = std::move(candidate);
		}
		else if (FAILED(backend.GetNativeDevice()->GetDeviceRemovedReason()))
		{
			shadowPreparation.failureKind = FailureKind::DeviceLost;
			return shadowPreparation;
		}
	}

	return {};
}

} // namespace

Result SceneOutputResources::Prepare(
	const RenderView& view, backend::Backend& backend, DrawResources& draws,
	SceneOutputImages*& images, Result& shadowPreparation, bool needsShadow)
{
	// Query the output extent for the current RenderView.
	Extent2D extent;
	Result result = QueryOutputExtent(view, backend, draws, extent);

	if (!result.IsSuccess())
	{
		return result;
	}

	// Search for an cached output resource set for this output identity. 
	const auto existing = std::find_if(
		m_outputs.begin(), m_outputs.end(),
		[&view](const auto& entry){ return SameViewOutput(entry->output, view.output); });

	if (existing == m_outputs.end())
	{
		// Publish an empty owner before registering anything. If a later CPU
		// allocation throws, the handles remain reachable for retry/finalization.
		auto entry = std::make_unique<SceneOutputImages>();
		entry->output = view.output;
		m_outputs.push_back(std::move(entry));
		images = m_outputs.back().get();
	}
	else
	{
		images = existing->get();	// Use the existing cached output resource set.
	}

	// Prepare the HDR output for this View, creating or resizing it if necessary.
	result = EnsureHdrOutput(view, extent, backend, draws, *images);

	if (!result.IsSuccess())
	{
		return result;
	}

	// Prepare the display Material for this View, creating or updating it if the exposure has changed.
	result = EnsureDisplayMaterial(view, draws, m_displayPipeline, *images);

	if (!result.IsSuccess())
	{
		return result;
	}

	if (needsShadow)
	{
		// Prepare the Shadow depth image for this View, creating it if necessary.
		return EnsureShadowDepth(backend, *images, shadowPreparation);
	}

	return {};
}

Result SceneOutputResources::PruneOutputs(backend::Backend& backend, DrawResources& draws)
{
	// Lambda returns true for any output that is no longer valid for the current frame.
	const auto outputUnavailable = [&](const std::unique_ptr<SceneOutputImages>& entry)
	{
		if (const auto* window = std::get_if<WindowOutputHandle>(&entry->output))
		{// Cached WindowOutput is no longer valid if the backend cannot find it.
			return !backend.IsWindowOutputValid(*window);
		}

		// Cached RenderTargetTexture is no longer valid if the registry cannot find it for new use.
		return !draws.FindContent(std::get<RenderTargetTextureHandle>(entry->output));
	};

	const bool hasUnavailableOutput = std::any_of(m_outputs.begin(), m_outputs.end(), outputUnavailable);

	if (!hasUnavailableOutput)
	{
		return {};
	}

	// Deleting a cache owner also deletes its Shadow DSV/SRV heaps. Confirm idle
	// once for this exceptional cleanup, before releasing any physical owner.
	Result idle = backend.WaitIdle();

	if (!idle.IsSuccess())
	{
		return idle;
	}

	for (const auto& entry : m_outputs)
	{
		if (!outputUnavailable(entry))
		{
			continue;
		}

		// Destroy the existing resources for invalidated outputs.

		if (entry->displayMaterial != MaterialHandle{})
		{
			Result destroyed = draws.Destroy(entry->displayMaterial);

			if (!destroyed.IsSuccess())
			{
				return destroyed;
			}
		}

		if (entry->hdrColor != RenderTargetTextureHandle{})
		{
			Result destroyed = draws.Destroy(entry->hdrColor);

			if (!destroyed.IsSuccess())
			{
				return destroyed;
			}
		}
	}

	std::erase_if(m_outputs, outputUnavailable);
	return {};
}

} // namespace rendering::detail
