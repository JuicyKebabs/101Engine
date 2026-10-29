#pragma once

#include "RenderGraph.h"
#include <span>
namespace rendering { class DrawCommand; }
namespace rendering::backend { class CommandContext; }

//---------------------------------------------------------------------------------------------------------------
// RenderPassContext
// This class provides the context for executing a single render pass within the render graph.
// It allows the render pass to access information about the textures used in the pass and their intended usage.
// It also provides access to the output layout of the pass.
//---------------------------------------------------------------------------------------------------------------

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

	Result RecordDrawCommands(std::span<const rendering::DrawCommand> commands);

private:
	RenderPassContext(
		const RenderGraph& graph,
		const CompiledPass& pass,
		backend::CommandContext* commands = nullptr,
		std::vector<std::uint64_t*>* fences = nullptr) noexcept
		: m_graph(graph), m_pass(pass), m_commands(commands), m_drawFences(fences)
	{}

	const RenderGraph& m_graph;
	const CompiledPass& m_pass;
	backend::CommandContext* m_commands = nullptr;
	std::vector<std::uint64_t*>* m_drawFences = nullptr;

	Result m_status;

private:
	Result Reject(const char* reason);

	friend class GraphExecutor;
};

}