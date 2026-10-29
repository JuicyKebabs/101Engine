#include "SceneFrameResources.h"
#include "BuiltinShaderSources.h"
#include "FrameRenderer.h"
#include "Failure.h"

#include <algorithm>
#include <cstring>
#include <d3dcompiler.h>

namespace rendering::detail
{

namespace
{

Result RegisterBuiltin(const char* source, ShaderStage stage, DrawResources& draws, ShaderHandle& handle)
{
    Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    const char* profile = stage == ShaderStage::Vertex ? "vs_5_0" : "ps_5_0";
    const HRESULT compiled = D3DCompile(source, std::strlen(source), "RendererBuiltin", nullptr,
        nullptr, "main", profile, D3DCOMPILE_ENABLE_STRICTNESS, 0, &bytecode, &errors);
    if (FAILED(compiled))
    {
        std::string message = "Cannot compile Renderer built-in Shader.";
        if (errors)
        {
            message.append(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        }

        return Fail(message);
    }

    ShaderDesc description;
    description.stage = stage;
    description.bytecode = std::span(static_cast<const std::byte*>(bytecode->GetBufferPointer()), bytecode->GetBufferSize());
    ResourceRegistrationResult<ShaderHandle> registration = draws.RegisterShader(description);
    if (!registration.result.IsSuccess())
    {
        return registration.result;
    }

    handle = registration.handle;
    return {};
}

}

Result SceneFrameResources::Initialize(backend::Backend& backend, DrawResources& draws)
{
    // A valid, clearable one-pixel depth image provides a safe binding when a
    // View has no light or its full-sized Shadow allocation cannot be prepared.
    Result result = m_disabledShadow.Ensure(backend.GetNativeDevice(), 1, 1, true);
    if (!result.IsSuccess())
    {
        return result;
    }

    ShaderHandle fullscreenVertex;
    result = RegisterBuiltin(BuiltinFullscreenShader, ShaderStage::Vertex, draws, fullscreenVertex);
    if (!result.IsSuccess())
    {
        return result;
    }

    ShaderHandle displayPixel;
    result = RegisterBuiltin(BuiltinDisplayShader, ShaderStage::Pixel, draws, displayPixel);
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
    result = RegisterBuiltin(BuiltinMeshShader, ShaderStage::Vertex, draws, shadowVertex);
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

Result SceneFrameResources::Prepare(const RenderView& view, backend::Backend& backend,
    RenderTargetTextureRegistry& targets, DrawResources& draws,
    SceneOutputImages*& images, Result& shadowPreparation, bool needsShadow)
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
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

    const auto existing = std::find_if(m_outputs.begin(), m_outputs.end(),
        [&view](const auto& entry) { return SameViewOutput(entry->output, view.output); });
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

    if (images->hdrColor == RenderTargetTextureHandle{})
    {
        RenderTargetTextureDesc description;
        description.width = width;
        description.height = height;
        description.format = TextureFormat::RGBA16_Float;
        // The private HDR image is normally cleared with its source View's
        // background. This is fixed at creation, independently of later Passes.
        description.optimizedClearColor = view.clearColor;
        ResourceRegistrationResult<RenderTargetTextureHandle> registration = targets.Create(backend.GetNativeDevice(), description);
        if (!registration.result.IsSuccess())
        {
            return registration.result;
        }

        images->hdrColor = registration.handle;
    }
    else
    {
        const auto* hdr = targets.FindForNewUse(images->hdrColor);
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

            result = targets.Resize(backend.GetNativeDevice(), images->hdrColor, width, height);
            if (!result.IsSuccess())
            {
                return result;
            }
        }
    }

    if (images->displayMaterial == MaterialHandle{} || images->exposure != view.exposure)
    {
        MaterialParameter textureParameter;
        textureParameter.name = "HDRTexture";
        textureParameter.value = images->hdrColor;
        MaterialParameter exposureParameter;
        exposureParameter.name = "Exposure";
        exposureParameter.value = view.exposure;

        MaterialDesc description;
        description.pipeline = m_displayPipeline;
        description.parameters.push_back(textureParameter);
        description.parameters.push_back(exposureParameter);
        if (images->displayMaterial == MaterialHandle{})
        {
            ResourceRegistrationResult<MaterialHandle> registration = draws.RegisterMaterial(description);
            if (!registration.result.IsSuccess())
            {
                return registration.result;
            }

            images->displayMaterial = registration.handle;
        }
        else
        {
            result = draws.UpdateMaterial(images->displayMaterial, description);
            if (!result.IsSuccess())
            {
                return result;
            }
        }

        images->exposure = view.exposure;
    }

    if (needsShadow && !images->shadowDepth)
    {
        auto candidate = std::make_unique<backend::DepthTexture>();
        shadowPreparation = candidate->Ensure(backend.GetNativeDevice(), 4096, 4096, true);
        if (shadowPreparation.IsSuccess())
        {
            images->shadowDepth = std::move(candidate);
        }
        else if (FAILED(backend.GetNativeDevice()->GetDeviceRemovedReason()))
        {
            shadowPreparation.failureKind = FailureKind::DeviceLost;
            return shadowPreparation;
        }
    }

    return {};
}

Result SceneFrameResources::Prune(backend::Backend& backend, RenderTargetTextureRegistry& targets,
    DrawResources& draws)
{
    const auto outputUnavailable = [&](const std::unique_ptr<SceneOutputImages>& entry)
    {
        if (const auto* window = std::get_if<WindowOutputHandle>(&entry->output))
        {
            return !backend.IsWindowOutputValid(*window);
        }

        return !targets.FindForNewUse(std::get<RenderTargetTextureHandle>(entry->output));
    };

    if (!std::any_of(m_outputs.begin(), m_outputs.end(), outputUnavailable))
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

}
