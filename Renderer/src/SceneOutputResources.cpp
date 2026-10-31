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
    ResourceRegistrationResult<PipelineHandle> displayRegistration = draws.RegisterPipeline(displayDescription);

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
    shadowDescription.cull = CullMode::Back;
    ResourceRegistrationResult<PipelineHandle> shadowRegistration = draws.RegisterPipeline(shadowDescription);

    if (!shadowRegistration.result.IsSuccess())
    {
        return shadowRegistration.result;
    }

    m_shadowPipeline = shadowRegistration.handle;
    return {};
}

namespace
{

Result QueryOutputExtent(const RenderView& view, backend::Backend& backend, RenderTargetTextureRegistry& targets,
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
        const auto* output = targets.FindForNewUse(std::get<RenderTargetTextureHandle>(view.output));

        if (!output || !output->gpuData)
        {
            return Fail("Scene output is unavailable; fallback sampling cannot replace an output attachment.");
        }

        width = output->gpuData->width;
        height = output->gpuData->height;
    }

    return {};
}

Result EnsureHdrOutput(const RenderView& view, const Extent2D& extent, backend::Backend& backend,
                       RenderTargetTextureRegistry& targets, SceneOutputImages& images)
{
    const std::uint32_t width = extent.width;
    const std::uint32_t height = extent.height;
    Result result;

    if (images.hdrColor == RenderTargetTextureHandle{})
    {
        RenderTargetTextureDesc description;
        description.width = width;
        description.height = height;
        description.format = TextureFormat::RGBA16_Float;
        // The private HDR image is normally cleared with its source View's
        // background. This is fixed at creation, independently of later Passes.
        description.optimizedClearColor = view.clearColor;
        ResourceRegistrationResult<RenderTargetTextureHandle> registration =
            targets.Create(backend.GetNativeDevice(), description);

        if (!registration.result.IsSuccess())
        {
            return registration.result;
        }

        images.hdrColor = registration.handle;
    }
    else
    {
        const auto* hdr = targets.FindForNewUse(images.hdrColor);
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

            result = targets.Resize(backend.GetNativeDevice(), images.hdrColor, width, height);

            if (!result.IsSuccess())
            {
                return result;
            }
        }
    }

    return {};
}

Result EnsureDisplayMaterial(const RenderView& view, DrawResources& draws, PipelineHandle displayPipeline,
                             SceneOutputImages& images)
{
    Result result;

    if (images.displayMaterial == MaterialHandle{} || images.exposure != view.exposure)
    {
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
        {
            ResourceRegistrationResult<MaterialHandle> registration = draws.RegisterMaterial(description);

            if (!registration.result.IsSuccess())
            {
                return registration.result;
            }

            images.displayMaterial = registration.handle;
        }
        else
        {
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
    {
        auto candidate = std::make_unique<backend::DepthTexture>();
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

Result SceneOutputResources::Prepare(const RenderView& view, backend::Backend& backend,
                                     RenderTargetTextureRegistry& targets, DrawResources& draws,
                                     SceneOutputImages*& images, Result& shadowPreparation, bool needsShadow)
{
    Extent2D extent;
    Result result = QueryOutputExtent(view, backend, targets, extent);

    if (!result.IsSuccess())
    {
        return result;
    }

    const auto existing = std::find_if(m_outputs.begin(), m_outputs.end(), [&view](const auto& entry)
                                       { return SameViewOutput(entry->output, view.output); });

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
        images = existing->get();
    }

    result = EnsureHdrOutput(view, extent, backend, targets, *images);

    if (!result.IsSuccess())
    {
        return result;
    }

    result = EnsureDisplayMaterial(view, draws, m_displayPipeline, *images);

    if (!result.IsSuccess())
    {
        return result;
    }

    if (needsShadow)
    {
        return EnsureShadowDepth(backend, *images, shadowPreparation);
    }

    return {};
}

Result SceneOutputResources::PruneOutputs(backend::Backend& backend, RenderTargetTextureRegistry& targets,
                                          DrawResources& draws)
{
    // Lambda returns true for any output that is no longer valid for the current frame.
    const auto outputUnavailable = [&](const std::unique_ptr<SceneOutputImages>& entry)
    {
        if (const auto* window = std::get_if<WindowOutputHandle>(&entry->output))
        {
            return !backend.IsWindowOutputValid(*window);
        }

        return !targets.FindForNewUse(std::get<RenderTargetTextureHandle>(entry->output));
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
            Result destroyed = draws.materials.Destroy(entry->displayMaterial);

            if (!destroyed.IsSuccess())
            {
                return destroyed;
            }
        }

        if (entry->hdrColor != RenderTargetTextureHandle{})
        {
            Result destroyed = targets.Destroy(entry->hdrColor);

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
