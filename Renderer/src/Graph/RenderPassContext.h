#pragma once

#include "RenderGraph.h"

namespace rendering::graph
{

struct PassTextureInfo
{
	GraphTextureFormat format = GraphTextureFormat::RGBA8_UNorm;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint32_t sampleCount = 1;
	std::uint32_t sampleQuality = 0;
};

class RenderPassContext
{
public:
	RenderPassContext(const RenderPassContext&) = delete;
	RenderPassContext& operator=(const RenderPassContext&) = delete;
	RenderPassContext(RenderPassContext&&) = delete;
	RenderPassContext& operator=(RenderPassContext&&) = delete;

	[[nodiscard]]
	const OutputLayout& GetOutputLayout() const noexcept { return m_pass.outputLayout; }

	// Used in execution of a render pass to get the information of a texture used in the pass.
	// Returns the texture info from temporary storage in the graph if execution requires the texture with registrered usage.
	Result GetTextureInfo(GraphTextureHandle texture, TextureUse intendedUse, PassTextureInfo& output);

private:
	RenderPassContext(const RenderGraph& graph, const CompiledPass& pass) noexcept
		: m_graph(graph), m_pass(pass) {}

	// Not changed during the execution of a render pass.
	const RenderGraph& m_graph;
	const CompiledPass& m_pass;

	Result m_status;

private:
	Result Reject(const char* reason);

	friend class GraphExecutor;
};

}