#pragma once
#include <memory>

#include "RendererApi.h"
#include "RendererConfiguration.h"
#include "Result.h"
#include "WindowOutput.h"
#include "ResourceHandle.h"
#include "Geometry.h"
#include "Texture.h"
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

	// Submit takes ownership only after acceptance. Render consumes one submission.
	[[nodiscard]] RENDERER_API Result SubmitFrameRenderData(FrameRenderData&& frame);
	[[nodiscard]] RENDERER_API RenderResult Render();

	// Registration APIs for resources
	[[nodiscard]] RENDERER_API GeometryRegistrationResult RegisterGeometry(const GeometryDesc& desc);
	[[nodiscard]] RENDERER_API TextureRegistrationResult RegisterTexture(const TextureDesc& desc);
	
	// Destruction APIs for resources
	RENDERER_API Result DestroyGeometry(GeometryHandle handle);
	RENDERER_API Result DestroyTexture(TextureHandle handle);

private:
	// Pimpl idiom to hide implementation details from the user of the Renderer class.
	struct impl;
	std::unique_ptr<impl> m_pImpl;
};

}
