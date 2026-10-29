// R7 implementation guide: section 4.17.
// Per-Render map connecting registry-owned resources and Graph imports.
// Material reads and View writes reuse one import when they refer to the same physical image.
// All resource pointers are borrowed until the containing Render/Execute finishes.

#pragma once

#include "GraphExecutor.h"

namespace rendering::detail
{
struct TextureContent;
struct RenderTargetTextureContent;
}

namespace rendering::backend
{
class DepthTexture;
}

namespace rendering::graph
{

struct ViewTargets
{
	GraphTextureHandle color;
	GraphTextureHandle depth;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

// Borrowed connections for one Graph. Owners must survive until execution ends.
class FrameResourceBindings
{
public:
	FrameResourceBindings(backend::Backend& backend, RenderGraph& graph, GraphExecutor& executor) noexcept;

	Result PrepareViewTargets(WindowOutputHandle output, backend::DepthTexture& depth, ViewTargets& targets, bool includeDepth = true);
	Result PrepareRenderTargetTargets(detail::RenderTargetTextureContent& content, bool includeDepth, ViewTargets& targets);
	Result ImportTexture(detail::TextureContent& content, const std::string& name, GraphTextureHandle& texture);
	Result FindImportedTexture(const detail::TextureContent& content, GraphTextureHandle& texture) const;
	Result ImportRenderTarget(detail::RenderTargetTextureContent& content, const std::string& name, GraphTextureHandle& texture);
	Result FindImportedRenderTarget(const detail::RenderTargetTextureContent& content, GraphTextureHandle& texture) const;

private:
	Result ImportBinding(const GraphTextureBinding& binding, const std::string& name, GraphTextureHandle& texture);
	Result ImportWindowOutput(WindowOutputHandle output, GraphTextureHandle& texture, ViewTargets& targets);
	Result ImportDepth(backend::DepthTexture& depth, GraphTextureHandle& texture);

	backend::Backend& m_backend;
	RenderGraph& m_graph;
	GraphExecutor& m_executor;
	std::vector<GraphTextureBinding> m_importedBindings;
};

}
