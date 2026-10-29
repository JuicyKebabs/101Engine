// R7 implementation guide: section 4.5.
// Public Renderer boundary: native resources and descriptors stay behind typed handles.
// Calls are single-threaded and public API reentry during Render is rejected.
// Resource mutation before Render affects a submitted frame; GPU lifetime is tracked separately.

#pragma once
#include <memory>

#include "RendererApi.h"
#include "RendererConfiguration.h"
#include "Result.h"
#include "WindowOutput.h"
#include "ResourceHandle.h"
#include "Geometry.h"
#include "Texture.h"
#include "RenderTargetTexture.h"
#include "Renderer/FrameRenderData.h"
#include "RenderResult.h"

//---------------------------------------------------------------------------
// Renderer class
// User instantiates this class to use the Rendering systems that we provide.
//---------------------------------------------------------------------------

namespace rendering
{

struct Win32WindowOutputDesc;

// Do not export the whole class to avoid issues by the exporting unique_ptr.
class Renderer
{
public:
	RENDERER_API Renderer();
	RENDERER_API ~Renderer();

	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;
	Renderer& operator=(Renderer&&) = delete;

	RENDERER_API Result Initialize(const RendererConfiguration& configuration);
	RENDERER_API void Finalize();

	RENDERER_API WindowOutputCreationResult CreateWindowOutput(const Win32WindowOutputDesc& outputDesc);
	// Synchronous replacement outside Render: wait for known GPU completion, then release old BackBuffers.
	// A partial DXGI failure leaves the logical Window alive but unavailable until a positive-size retry.
	RENDERER_API Result ResizeWindowOutput(WindowOutputHandle handle, std::uint32_t width, std::uint32_t height);

	// Render targets own GPU-generated output; ordinary Texture APIs accept CPU pixels.
	// These APIs operate on the separate render-target owner, not CPU-upload TextureRegistry.
	// Material reads use this same handle; no extra read-only Texture handle is issued.
	// Resize preserves identity, replaces the complete physical set and invalidates its contents on success.
	RENDERER_API ResourceRegistrationResult<RenderTargetTextureHandle> CreateRenderTargetTexture(const RenderTargetTextureDesc& desc);
	RENDERER_API Result ResizeRenderTargetTexture(RenderTargetTextureHandle handle, std::uint32_t width, std::uint32_t height);
	RENDERER_API Result DestroyRenderTargetTexture(RenderTargetTextureHandle handle);
	RENDERER_API Result GetRenderTargetTextureInfo(RenderTargetTextureHandle handle, RenderTargetTextureInfo& output) const;

	// Submit takes ownership of Frame values only after acceptance. Render consumes one submission.
	// Resource contents are resolved during Render: intervening Updates affect that submission.
	// Destroyed dependencies exclude the affected Object with a diagnostic.
	// Call from one thread; public API reentry during Render is prohibited.
	RENDERER_API Result SubmitFrameRenderData(FrameRenderData&& frame);
	RENDERER_API RenderResult Render();

	// Registration APIs for resources
	RENDERER_API GeometryRegistrationResult RegisterGeometry(const GeometryDesc& desc);
	RENDERER_API TextureRegistrationResult RegisterTexture(const TextureDesc& desc);
	RENDERER_API ResourceRegistrationResult<ShaderHandle> RegisterShader(const ShaderDesc& desc);
	RENDERER_API ResourceRegistrationResult<PipelineHandle> RegisterPipeline(const PipelineDesc& desc);
	RENDERER_API ResourceRegistrationResult<MaterialHandle> RegisterMaterial(const MaterialDesc& desc);
	
	// Destruction APIs for resources
	RENDERER_API Result DestroyGeometry(GeometryHandle handle);
	RENDERER_API Result DestroyTexture(TextureHandle handle);
	RENDERER_API Result DestroyShader(ShaderHandle handle);
	RENDERER_API Result DestroyPipeline(PipelineHandle handle);
	RENDERER_API Result DestroyMaterial(MaterialHandle handle);

	// Update APIs for resources
	RENDERER_API Result UpdateGeometry(GeometryHandle handle,const GeometryDesc& desc);
	RENDERER_API Result UpdateTexture(TextureHandle handle, const TextureDesc& desc);
	RENDERER_API Result UpdateMaterial(MaterialHandle handle, const MaterialDesc& desc);

	// Query APIs for resources
	RENDERER_API Result GetShaderMetadata(ShaderHandle handle,ShaderMetadata& output) const;

private:
	// Pimpl idiom to hide implementation details from the user of the Renderer class.
	struct impl;
	std::unique_ptr<impl> m_pImpl;
};

}
