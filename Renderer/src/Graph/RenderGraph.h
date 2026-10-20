#pragma once

#include "RenderPass.h"

#include <optional>
#include <vector>

namespace rendering::graph
{

struct PlannedTransition
{
	GraphTextureHandle texture;
	TextureState before;
	TextureState after;
};

struct CompiledPass
{
	std::uint32_t sourcePass = 0;
	std::vector<PassTextureUse> uses;
	std::vector<ColorAttachment> colors;
	std::optional<DepthAttachment> depth;
	std::vector<PlannedTransition> transitions;
};

struct PlannedTextureResult
{
	GraphTextureHandle texture;
	TextureState state;
	bool contentsValid = false;
};

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
};

}