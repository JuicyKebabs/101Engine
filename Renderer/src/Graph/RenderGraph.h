#pragma once

#include "RenderPass.h"
#include "OutputLayout.h"

#include <optional>
#include <vector>

namespace rendering::graph
{

struct PlannedTransition
{
	GraphTextureHandle texture;	// Target to transition.
	TextureState before;		// State before the transition.
	TextureState after;			// State after the transition.
};

// A pass which is validated by RenderGraph.
// State transition, Target, and output information are resolved and ready for execution.
// Multiple transition for the same texture is not supported in a single pass. (should be separated into multiple passes.)
struct CompiledPass
{
	OutputLayout outputLayout;
	std::uint32_t sourcePass = 0;
	std::vector<PassTextureUse> uses;
	std::vector<ColorAttachment> colors;
	std::optional<DepthAttachment> depth;
	std::vector<PlannedTransition> transitions;
};

// Represents the final state of a texture used in the render graph.
struct PlannedTextureResult
{
	GraphTextureHandle texture;
	TextureState state;
	bool contentsValid = false;
};

// RenderGraph executes a rendering sequence based on this plan.
// This holds every compiled pass that will be executed, the texture resources,
// and their final state at the end of the render graph execution.
struct CompiledPlan
{
	std::vector<CompiledPass> passes;
	std::vector<PlannedTransition> finalTransitions;
	std::vector<PlannedTextureResult> textures;
};

class RenderGraph
{
public:
	RenderGraph() noexcept;
	RenderGraph(const RenderGraph&) = delete;
	RenderGraph& operator=(const RenderGraph&) = delete;
	RenderGraph(RenderGraph&&) = delete;
	RenderGraph& operator=(RenderGraph&&) = delete;

	Result ImportTexture(const ImportedTextureDesc& desc, GraphTextureHandle& output);
	Result SetFinalUsage(GraphTextureHandle texture, TextureUse use);
	Result AddPass(RenderPass&& pass);
	Result Compile();

	// Borrowed metadata. Do not retain across mutation; Import may reallocate the table.
	[[nodiscard]] const ImportedTextureDesc* FindTexture(GraphTextureHandle texture) const noexcept;

	// Inspection only. Borrowed until any mutation/Compile or Graph destruction.
	[[nodiscard]] const CompiledPlan* GetCompiledPlan() const noexcept
	{
		return m_plan ? &*m_plan : nullptr;
	}

private:
	struct ResourceEntry
	{
		ImportedTextureDesc desc;			// Description of the imported texture resource (Including pointer to the actual resource).
		std::optional<TextureUse> finalUse;	// The final use of the texture within the render graph, if any.
	};

	std::uint64_t m_id = 0;
	std::vector<ResourceEntry> m_resources;
	std::vector<RenderPass> m_passes;
	std::optional<CompiledPlan> m_plan;

	bool m_executionStarted = false;	// If this graph is tried to be executed (regardless of successful execution or not).
	bool m_executing = false;			// If this graph is currently being executed (true only while successful execution).
	bool m_executionViolated = false;	// If this graph's execution violated any rules (executing becomes false. started is kept being true).

private:
	Result CheckMutable();

	friend class GraphExecutor;
};

}