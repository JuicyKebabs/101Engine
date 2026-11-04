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

//----------------------------------------------------------------------------
// DrawResources
//----------------------------------------------------------------------------

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
	explicit DrawResources(std::uint32_t owner) :
		m_geometryResources(owner),
		m_textureResources(owner),
		m_renderTargetResources(owner),
		m_shaderResources(owner),
		m_pipelineResources(owner),
		m_materialResources(owner) {}

	// The Renderer owns the resident fallback resources, which are never destroyed.
	Result InitializeFallbacks();
	GeometryHandle fallbackGeometry;
	TextureHandle fallbackTexture;
	MaterialHandle fallbackMaterial;

	Result RegisterBuiltinShader(const char* source, ShaderStage stage, ShaderHandle& handle);

	ResourceRegistrationResult<GeometryHandle> Register(const GeometryDesc& desc);
	ResourceRegistrationResult<TextureHandle> Register(const TextureDesc& desc);
	ResourceRegistrationResult<ShaderHandle> Register(const ShaderDesc& desc);
	ResourceRegistrationResult<PipelineHandle> Register(const PipelineDesc& desc);
	ResourceRegistrationResult<MaterialHandle> Register(const MaterialDesc& desc);
	Result UpdateMaterial(MaterialHandle handle, const MaterialDesc& desc);

	Result ResolveDrawInputs(
		const RenderObject& object,
		std::size_t objectIndex,
		const DrawSettings& settings,
		DrawInputs& inputs,
		DrawDiagnostic& diagnostic);

	Result ResolveTextureInput(
		const MaterialDesc& material,
		const ShaderResourceBinding& requirement,
		TextureHandle& handle,
		TextureContent*& content);

	Result ResolveSampledTexture(
		const MaterialDesc& material,
		const ShaderResourceBinding& requirement,
		SampledTexture& sampled);

	Result PrepareCommands(
		FrameDrawContext& drawContext, const PassRecordingInputs& pass,
		std::span<const PlannedDraw> draws, const graph::OutputLayout& output,
		std::vector<DrawCommand>& commands);

	GeometryContent* FindContent(GeometryHandle handle) { return m_geometryResources.FindRegisteredContent(handle); }
	TextureContent* FindContent(TextureHandle handle) { return m_textureResources.FindRegisteredContent(handle); }
	RenderTargetTextureContent* FindContent(RenderTargetTextureHandle handle) { return m_renderTargetResources.FindForNewUse(handle); }
	ShaderContent* FindContent(ShaderHandle handle) { return m_shaderResources.FindForNewUse(handle); }
	PipelineContent* FindContent(PipelineHandle handle) { return m_pipelineResources.FindForNewUse(handle); }
	MaterialContent* FindContent(MaterialHandle handle) { return m_materialResources.FindForNewUse(handle); }

	Result Destroy(GeometryHandle handle) { return m_geometryResources.Destroy(handle); }
	Result Destroy(TextureHandle handle) { return m_textureResources.Destroy(handle); }
	Result Destroy(RenderTargetTextureHandle handle) { return m_renderTargetResources.Destroy(handle); }
	Result Destroy(ShaderHandle handle) { return m_shaderResources.Destroy(handle); }
	Result Destroy(PipelineHandle handle) { return m_pipelineResources.Destroy(handle); }
	Result Destroy(MaterialHandle handle) { return m_materialResources.Destroy(handle); }

	Result Update(GeometryHandle handle, const GeometryDesc& desc) { return m_geometryResources.Update(handle, desc); }
	Result Update(TextureHandle handle, const TextureDesc& desc) { return m_textureResources.Update(handle, desc); }


	void CollectgeometryDestroyed() { m_geometryResources.CollectDestroyed(); }
	void CollectTextureDestroyed() { m_textureResources.CollectDestroyed(); }

	bool MarkPrepared(GeometryHandle handle, GeometryContent& content) { return m_geometryResources.MarkPrepared(handle, content); }
	bool MarkPrepared(TextureHandle handle, TextureContent& content) { return m_textureResources.MarkPrepared(handle, content); }
	bool MarkPreparationFailed(GeometryHandle handle, GeometryContent& content) { return m_geometryResources.MarkPreparationFailed(handle, content); }
	bool MarkPreparationFailed(TextureHandle handle, TextureContent& content) { return m_textureResources.MarkPreparationFailed(handle, content); }

	Result GetGeometryData(GeometryHandle handle, GeometryData& output) const { return m_geometryResources.GetData(handle, output); }

	// Wrappers for RenderTargetTextureRegistry.
	ResourceRegistrationResult<RenderTargetTextureHandle> CreateRenderTargetTexture(ID3D12Device* device, const RenderTargetTextureDesc& description) { return m_renderTargetResources.Create(device, description); }
	Result ResizeRenderTargetTexture(ID3D12Device* device, RenderTargetTextureHandle handle, std::uint32_t width, std::uint32_t height) { return m_renderTargetResources.Resize(device, handle, width, height); }
	Result QueryRenderTargetTextureInfo(RenderTargetTextureHandle handle, RenderTargetTextureInfo& output) const { return m_renderTargetResources.QueryInfo(handle, output); }
	
	std::string DescribeTextureUnavailable(TextureHandle handle) const { return m_textureResources.DescribeUnavailable(handle); }
	std::string DescribeRenderTargetTextureUnavailable(RenderTargetTextureHandle handle) const { return m_renderTargetResources.DescribeUnavailable(handle); }

private:
	GeometryRegistry m_geometryResources;
	TextureRegistry m_textureResources;
	RenderTargetTextureRegistry m_renderTargetResources;
	VersionedRegistry<ShaderHandle, ShaderContent> m_shaderResources;
	VersionedRegistry<PipelineHandle, PipelineContent> m_pipelineResources;
	VersionedRegistry<MaterialHandle, MaterialContent> m_materialResources;

private:
	Result GetOrCreatePipelineVariant(
		const DrawInputs& inputs, const DrawDiagnostic& diagnostic,
		const graph::OutputLayout& output, backend::Backend& backend,
		PipelineVariant*& result);

	Result PrepareConstantBindings(
		const FrameRenderData& frame, const PassRecordingInputs& pass,
		const RenderObject& object, const DrawInputs& inputs, FrameDrawContext& drawContext,
		std::array<backend::ConstantAllocation, 4>& commonAllocations, DrawCommand& command);

	Result PrepareTextureBindings(
		const DrawInputs& inputs, FrameDrawContext& drawContext,
		const PassRecordingInputs& pass, DrawCommand& command);
};

Result ValidateFrameInputs(const FrameRenderData& frame);

} // namespace rendering::detail
