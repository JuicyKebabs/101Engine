#include "DrawSystem.h"
#include "FrameDrawContext.h"
#include "ShaderCommonInputs.h"
#include "Math/Finite.h"
#include "Failure.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace rendering::detail
{

namespace
{

Result DeriveObjectWorld(const RenderObject& object, const RenderView& view, Matrix4x4& output)
{
    output = object.world;

    if (object.billboard == BillboardMode::None)
    {
        return {};
    }

    const float viewDeterminant = view.view.Determinant();

    if (!std::isfinite(viewDeterminant) || std::abs(viewDeterminant) < 1e-12f)
    {
        return Fail("Billboard requires an invertible View matrix.");
    }

    // Camera up must come from this View, including Camera roll.
    // Using a global main Camera would mix facing between Views; using world Y here would lose spherical roll.
    const Matrix4x4 cameraWorld = view.view.Inverse();
    Vector3 cameraUp;
    cameraUp.x = cameraWorld._21;
    cameraUp.y = cameraWorld._22;
    cameraUp.z = cameraWorld._23;
    cameraUp = cameraUp.Normalized();

    // The spherical helper needs a nonparallel Up/forward pair.
    // Choose a deterministic perpendicular axis at the camera/object pole.
    const Vector3 objectPosition = object.world.GetTranslation();
    const Vector3 cameraDirection = view.cameraPosition - objectPosition;

    if (object.billboard == BillboardMode::Spherical && cameraDirection.LengthSq() > 1e-6f)
    {
        const Vector3 forward = cameraDirection.Normalized();

        if (cameraUp.Cross(forward).LengthSq() <= 1e-6f)
        {
            cameraUp = std::abs(forward.y) < 0.9f ? Vector3::Up() : Vector3::Right();
        }
    }

    Matrix4x4 sourceWorld = object.world;

    if (object.billboard == BillboardMode::Spherical)
    {
        output = sourceWorld.ToBillboard(view.cameraPosition, cameraUp);
    }
    else if (object.billboard == BillboardMode::Cylindrical)
    {
        output = sourceWorld.ToCylindricalBillboard(view.cameraPosition, cameraUp);
    }
    else
    {
        return Fail("Unsupported Billboard mode.");
    }

    const float resultDeterminant = output.Determinant();

    if (!Finite(output) || !std::isfinite(resultDeterminant) || std::abs(resultDeterminant) < 1e-12f)
    {
        return Fail("Billboard produced an invalid World matrix.");
    }

    return {};
}

D3D12_RECT DrawScissor(const RenderObject& object, const Viewport& viewport, std::uint32_t width, std::uint32_t height)
{
    D3D12_RECT scissor{};
    scissor.left = (std::max)(LONG{0}, static_cast<LONG>(std::floor(viewport.x)));
    scissor.top = (std::max)(LONG{0}, static_cast<LONG>(std::floor(viewport.y)));
    scissor.right = (std::min)(static_cast<LONG>(width), static_cast<LONG>(std::ceil(viewport.x + viewport.width)));
    scissor.bottom = (std::min)(static_cast<LONG>(height), static_cast<LONG>(std::ceil(viewport.y + viewport.height)));

    if (object.scissor)
    {
        scissor.left = (std::max)(scissor.left, static_cast<LONG>(object.scissor->left));
        scissor.top = (std::max)(scissor.top, static_cast<LONG>(object.scissor->top));
        scissor.right = (std::min)(scissor.right, static_cast<LONG>(object.scissor->right));
        scissor.bottom = (std::min)(scissor.bottom, static_cast<LONG>(object.scissor->bottom));
    }

    return scissor;
}

} // namespace

// Allocate this View's Camera and per-Object data in distinct frame storage.
// Common allocations may be shared inside the current preparation call, never across different Views.
// Compute both World and inverse-transpose normal data from the derived Billboard World.
Result DrawResources::PrepareConstantBindings(const FrameRenderData& frame, const PassRecordingInputs& pass,
                                              const RenderObject& object, const DrawInputs& inputs,
                                              FrameDrawContext& drawContext,
                                              std::array<backend::ConstantAllocation, 4>& commonAllocations,
                                              DrawCommand& command)
{
    ViewConstants viewConstants;
    viewConstants.view = pass.view->view;
    viewConstants.projection = pass.view->projection;
    viewConstants.cameraPosition.x = pass.view->cameraPosition.x;
    viewConstants.cameraPosition.y = pass.view->cameraPosition.y;
    viewConstants.cameraPosition.z = pass.view->cameraPosition.z;
    viewConstants.cameraPosition.w = 0;

    viewConstants.shadowViewProjection = pass.shadow.viewProjection;
    viewConstants.shadowInformation.x = pass.shadow.enabled ? 1.0f : 0.0f;
    viewConstants.shadowInformation.y = 0.001f;
    viewConstants.shadowInformation.z = 0;
    viewConstants.shadowInformation.w = 0;

    ObjectConstants objectConstants;
    Result worldPrepared =
        DeriveObjectWorld(object, pass.billboardSource ? *pass.billboardSource : *pass.view, objectConstants.world);

    if (!worldPrepared.IsSuccess())
    {
        return worldPrepared;
    }

    // Normals must use the same derived World as positions.
    // Leaving the original normal matrix after Billboard rotation would produce inconsistent lighting.
    objectConstants.normalMatrix = objectConstants.world.Inverse().Transpose();
    objectConstants.tint = object.tint;

    if (inputs.geometrySubstituted)
    {
        Vector4 neutralTint;
        neutralTint.x = 1;
        neutralTint.y = 1;
        neutralTint.z = 1;
        neutralTint.w = 1;
        objectConstants.tint = neutralTint;
    }

    DirectionalLight lightConstants;

    if (frame.directionalLight.has_value())
    {
        lightConstants = *frame.directionalLight;
    }

    std::array<float, 4> frameConstants{};
    frameConstants[0] = frame.time;
    Vector4 passConstants;
    passConstants.x = 1;
    passConstants.y = 1;
    passConstants.z = 1;
    passConstants.w = 1;

    const auto bytes = [](const auto& value)
    {
        using ConstantType = std::remove_cvref_t<decltype(value)>;
        const std::span<const ConstantType> values(&value, 1);
        return std::as_bytes(values);
    };

    std::vector<std::byte> materialConstants;
    UINT rootIndex = 0;

    for (const auto& buffer : inputs.pipeline->layout.constants)
    {
        std::span<const std::byte> constantBytes;

        switch (buffer.binding)
        {
        case 0:
            constantBytes = bytes(frameConstants);
            break;
        case 1:
            constantBytes = bytes(viewConstants);
            break;
        case 2:
            constantBytes = bytes(lightConstants);
            break;
        case 3:
            constantBytes = bytes(passConstants);
            break;
        case 4:
            constantBytes = bytes(objectConstants);
            break;

        case 5:
        {
            Result result = PackMaterial(buffer, inputs.material->desc, materialConstants);

            if (!result.IsSuccess())
            {
                return result;
            }

            constantBytes = materialConstants;
            break;
        }
        default:
            return {false, "PrepareConstantBindings: Unsupported constant buffer binding."};
        }

        backend::ConstantAllocation allocation;

        // Only frame/View/light/pass bindings may share one allocation inside this View preparation call.
        // Object and Material constants need their own ranges. The cache is recreated for the next View.
        const bool isCommonBuffer = buffer.binding < commonAllocations.size();

        if (isCommonBuffer && commonAllocations[buffer.binding].gpuAddress)
        {
            allocation = commonAllocations[buffer.binding];
        }
        else
        {

            Result result = drawContext.Backend().AllocateConstants(constantBytes, allocation);

            if (!result.IsSuccess())
            {
                return result;
            }

            if (isCommonBuffer)
            {
                commonAllocations[buffer.binding] = allocation;
            }
        }

        backend::ConstantBinding binding;
        binding.rootIndex = rootIndex;
        binding.gpuAddress = allocation.gpuAddress;
        command.m_packet.constants.push_back(binding);
        ++rootIndex;
    }

    return {};
}

// Resolve the same owner that declaration imported, copy its CPU SRV to the frame-visible heap,
// while its registry/cache owner stays stable through synchronous execution.
// Descriptor copying creates a view description; the owner protects the underlying storage.
Result DrawResources::PrepareTextureBindings(const DrawInputs& inputs, FrameDrawContext& drawContext,
                                             const PassRecordingInputs& pass, DrawCommand& command)
{
    UINT rootIndex = static_cast<UINT>(command.m_packet.constants.size());

    for (const auto& requirement : inputs.pipeline->layout.resources)
    {
        if (requirement.sampler)
        {
            continue;
        }

        SampledTexture sampled;
        Result resolved = drawContext.ResolveSample(inputs.material->desc, requirement, sampled, pass);

        if (!resolved.IsSuccess())
        {
            return resolved;
        }

        graph::GraphTextureHandle graphTexture;
        D3D12_CPU_DESCRIPTOR_HANDLE sourceDescriptor{};

        if (const auto* target = std::get_if<RenderTargetInput>(&sampled))
        {
            resolved = drawContext.Bindings().FindImportedRenderTarget(*target->content, graphTexture);
            sourceDescriptor = target->content->gpuData->Srv();
        }
        else if (const auto* depth = std::get_if<backend::DepthTexture*>(&sampled))
        {
            resolved = drawContext.Bindings().FindImportedDepth(**depth, graphTexture);
            sourceDescriptor = (*depth)->Srv();
        }
        else
        {
            const auto& texture = std::get<UploadedTextureInput>(sampled);
            resolved = drawContext.Bindings().FindImportedTexture(*texture.content, graphTexture);
            sourceDescriptor = texture.content->gpuData->GetSrvHandle();
        }

        if (!resolved.IsSuccess())
        {
            return resolved;
        }

        D3D12_GPU_DESCRIPTOR_HANDLE descriptor{};
        Result copied = drawContext.Backend().CopyFrameDescriptor(sourceDescriptor, descriptor);

        if (!copied.IsSuccess())
        {
            return copied;
        }

        backend::TextureBinding binding;
        binding.rootIndex = rootIndex;
        binding.descriptor = descriptor;
        command.m_packet.textures.push_back(binding);
        ++rootIndex;

        graph::PassTextureUse read;
        read.texture = graphTexture;
        read.use.usage = graph::TextureUsage::ShaderRead;
        read.use.stages = static_cast<graph::ShaderStage>(static_cast<unsigned>(requirement.stages));
        read.requiresValidContents = true;
        command.m_reads.push_back(read);
    }

    return {};
}

// Prepare commands for one View's original Object indices and current output layout.
// Resolve effective viewport/scissor and remove empty clips before native recording.
// Allocation/preparation failure aborts the unsent Graph rather than submitting a partial frame.
Result DrawResources::PrepareCommands(FrameDrawContext& drawContext, const PassRecordingInputs& pass,
                                      std::span<const PlannedDraw> draws, const graph::OutputLayout& output,
                                      std::vector<DrawCommand>& commands)
{
    // This cache is deliberately local to one PrepareCommands call for one View.
    // Moving it to frame-global state would reuse Camera A addresses when preparing Camera B.
    std::array<backend::ConstantAllocation, 4> commonAllocations{};

    for (const PlannedDraw& draw : draws)
    {
        const std::size_t objectIndex = draw.objectIndex;
        const DrawSettings& settings = draw.settings;
        if (objectIndex >= drawContext.Frame().objects.size())
        {
            return {false, "PrepareCommands: Object index is out of range."};
        }

        DrawInputs inputs;
        DrawDiagnostic diagnostic;
        Result result = drawContext.ResolveObjectInputs(objectIndex, settings, inputs, diagnostic, pass);

        if (!result.IsSuccess())
        {
            drawContext.AddDiagnostic(diagnostic, "DrawResolve", result.message, pass);
            continue;
        }

        if (!drawContext.AreResourcesPrepared(inputs, pass))
        {
            drawContext.AddDiagnostic(diagnostic, "DrawResolve", "Selected Resource is not ready.", pass);
            continue;
        }

        const bool colorCountMismatch =
            inputs.pixelShader && inputs.pixelShader->metadata.colorOutputs != output.colorFormats.size();
        const bool missingPixelShader = !inputs.pixelShader && !output.colorFormats.empty();
        const bool missingDepth =
            inputs.depthOverride.value_or(inputs.pipeline->desc.depth) != DepthMode::Disabled && !output.depthFormat;

        if (colorCountMismatch || missingPixelShader || missingDepth)
        {
            drawContext.AddDiagnostic(diagnostic, "DrawResolve", "Pipeline does not match the OutputLayout.", pass);
            continue;
        }

        PipelineVariant* pipeline = nullptr;
        result = GetOrCreatePipelineVariant(inputs, diagnostic, output, drawContext.Backend(), pipeline);

        if (!result.IsSuccess())
        {
            return result;
        }

        DrawCommand command;
        command.m_packet.pipeline = pipeline->gpu.pipelineState.Get();
        command.m_packet.root = pipeline->gpu.rootSignature.Get();
        command.m_packet.heap = drawContext.Backend().GetFrameDescriptorHeap();

        const auto& object = drawContext.Frame().objects[objectIndex];

        if (object.procedural)
        {
            command.m_packet.requiresVertexBuffer = false;
            command.m_packet.count = object.procedural->vertexCount;
        }
        else
        {
            command.m_packet.vertices = inputs.geometry->gpuData->vertexView;
            command.m_packet.indices = inputs.geometry->gpuData->indexView;

            const bool indexed = inputs.geometry->gpuData->indexCount != 0;
            command.m_packet.kind = indexed ? backend::DrawKind::Indexed : backend::DrawKind::Vertices;
            const UINT availableCount =
                indexed ? inputs.geometry->gpuData->indexCount : inputs.geometry->gpuData->vertexCount;

            // The original range may describe a completely different mesh.
            // Applying it to the resident cube would hide it or access past VB.
            if (inputs.geometrySubstituted)
            {
                command.m_packet.first = 0;
                command.m_packet.baseVertex = 0;
                command.m_packet.count = availableCount;
            }
            else
            {
                command.m_packet.first = object.drawRange.first;
                command.m_packet.baseVertex = object.drawRange.baseVertex;
                command.m_packet.count =
                    object.drawRange.count == 0 ? availableCount - object.drawRange.first : object.drawRange.count;
            }
        }

        // Resolve clipping before allocating constants/descriptors or retaining native objects.
        // An empty clipped draw records nothing and must not leak its scissor into the following draw.
        command.m_packet.scissor = DrawScissor(object, pass.viewport, pass.extent.width, pass.extent.height);

        if (command.m_packet.scissor.right <= command.m_packet.scissor.left ||
            command.m_packet.scissor.bottom <= command.m_packet.scissor.top)
        {
            continue;
        }

        result =
            PrepareConstantBindings(drawContext.Frame(), pass, object, inputs, drawContext, commonAllocations, command);

        if (!result.IsSuccess())
        {
            return result;
        }

        result = PrepareTextureBindings(inputs, drawContext, pass, command);

        if (!result.IsSuccess())
        {
            return result;
        }

        commands.push_back(std::move(command));
    }

    return {};
}

} // namespace rendering::detail
