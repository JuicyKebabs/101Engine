#pragma once

#include "Resource/ShaderData.h"
#include "DrawCommand.h"
#include "DrawPlan.h"
#include "Graph/OutputLayout.h"
#include "Resource/GeometryRegistry.h"
#include "Resource/TextureRegistry.h"
#include "Resource/RenderTargetTextureRegistry.h"
#include "Resource/VersionedRegistry.h"
#include "Backend/Backend.h"
#include "Backend/PipelineGpu.h"
#include "Renderer/FrameRenderData.h"

#include <array>

namespace rendering::detail
{

class FrameDrawContext;

struct Pipelinekey
{
    PipelineHandle pipeline;
    ShaderHandle vertex;
    ShaderHandle pixel;
    std::uint64_t vertexRevision = 0;
    std::uint64_t pixelRevision = 0;
    std::uint32_t vertexLayout = 0;
    std::uint32_t pixelLayout = 0;
    std::vector<graph::GraphTextureFormat> colors;
    std::optional<graph::GraphTextureFormat> depth;
    std::uint32_t sampleCount = 1;
    DepthMode effectiveDepth = DepthMode::Disabled;
    std::uint32_t sampleQuality = 0;
    bool operator==(const Pipelinekey& other) const noexcept = default;
};

struct PipelineVariant
{
    Pipelinekey key;
    backend::PipelineGpuData gpu;
};

struct PipelineContent
{
    std::uint64_t revision = 1;
    PipelineDesc desc;
    ShaderMetadata layout;
    std::vector<PipelineVariant> variants;
};

struct MaterialContent
{
    std::uint64_t revision = 1;
    MaterialDesc desc;
};

// One lookup's borrowed results. Never stored as a second array of RenderObjects.
struct DrawInputs
{
    // Temporary resolution metadata. It never rewrites the submitted Object.
    bool geometrySubstituted = false;
    std::optional<DepthMode> depthOverride;
    GeometryContent* geometry = nullptr;
    MaterialContent* material = nullptr;
    PipelineContent* pipeline = nullptr;
    ShaderContent* vertexShader = nullptr;
    ShaderContent* pixelShader = nullptr;
};

// A successful resolution selects one typed owner. All pointers are Render-scope borrows.
struct UploadedTextureInput
{
    TextureHandle handle;
    TextureContent* content = nullptr;
};

struct RenderTargetInput
{
    RenderTargetTextureHandle handle;
    RenderTargetTextureContent* content = nullptr;
};

using SampledTexture = std::variant<std::monostate, UploadedTextureInput, RenderTargetInput, backend::DepthTexture*>;

using RenderList = std::vector<std::size_t>;

class DrawResources
{
public:
    explicit DrawResources(std::uint32_t owner) : shaders(owner), pipelines(owner), materials(owner) {}

    // Private resident resources have the Renderer lifetime, outside Scene sets.
    // Initialization must fail if the emergency display itself cannot be built.
    Result InitializeFallbacks(GeometryRegistry& geometries, TextureRegistry& textures);
    GeometryHandle fallbackGeometry;
    TextureHandle fallbackTexture;
    MaterialHandle fallbackMaterial;

    Result RegisterBuiltinShader(const char* source, ShaderStage stage, ShaderHandle& handle);

    ResourceRegistrationResult<ShaderHandle> RegisterShader(const ShaderDesc& desc);
    ResourceRegistrationResult<PipelineHandle> RegisterPipeline(const PipelineDesc& desc);
    ResourceRegistrationResult<MaterialHandle> RegisterMaterial(const MaterialDesc& desc);
    Result UpdateMaterial(MaterialHandle handle, const MaterialDesc& desc);

    Result ResolveDrawInputs(const RenderObject& object, std::size_t objectIndex, const DrawSettings& settings,
                             GeometryRegistry& geometries, TextureRegistry& textures, DrawInputs& inputs,
                             DrawDiagnostic& diagnostic, RenderTargetTextureRegistry* targets = nullptr);

    Result ResolveTextureInput(const MaterialDesc& material, const ShaderResourceBinding& requirement,
                               TextureRegistry& textures, TextureHandle& handle, TextureContent*& content) const;

    Result ResolveSampledTexture(const MaterialDesc& material, const ShaderResourceBinding& requirement,
                                 TextureRegistry& textures, RenderTargetTextureRegistry& targets,
                                 SampledTexture& sampled) const;

    Result PrepareCommands(FrameDrawContext& drawContext, const PassRecordingInputs& pass,
                           std::span<const PlannedDraw> draws, const graph::OutputLayout& output,
                           std::vector<DrawCommand>& commands);

    VersionedRegistry<ShaderHandle, ShaderContent> shaders;
    VersionedRegistry<PipelineHandle, PipelineContent> pipelines;
    VersionedRegistry<MaterialHandle, MaterialContent> materials;

private:
    Result GetOrCreatePipelineVariant(const DrawInputs& inputs, const DrawDiagnostic& diagnostic,
                                      const graph::OutputLayout& output, backend::Backend& backend,
                                      PipelineVariant*& result);

    Result PrepareConstantBindings(const FrameRenderData& frame, const PassRecordingInputs& pass,
                                   const RenderObject& object, const DrawInputs& inputs, FrameDrawContext& drawContext,
                                   std::array<backend::ConstantAllocation, 4>& commonAllocations, DrawCommand& command);

    Result PrepareTextureBindings(const DrawInputs& inputs, FrameDrawContext& drawContext,
                                  const PassRecordingInputs& pass, DrawCommand& command);
};

Result ValidateFrameInputs(const FrameRenderData& frame);

} // namespace rendering::detail
