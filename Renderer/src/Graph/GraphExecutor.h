#pragma once

#include "RenderGraph.h"
#include "GraphTextureBinding.h"
#include "Backend/Backend.h"

#include <vector>

//-----------------------------------------------------------------------------------------------------------------
// GraphExecutor
// This class connects the execution process planed by RenderGraph to the actual backend operations.
// Stores all bindings of the graph resources in a frame, and execute the logical plan by using backend operation.
//-----------------------------------------------------------------------------------------------------------------

namespace rendering::graph
{

class GraphExecutor
{
public:
	GraphExecutor(RenderGraph& graph, backend::Backend& backend) noexcept
		: m_graph(graph), m_backend(backend) {}
	GraphExecutor(const GraphExecutor&) = delete;
	GraphExecutor& operator=(const GraphExecutor&) = delete;
	GraphExecutor(GraphExecutor&&) = delete;
	GraphExecutor& operator=(GraphExecutor&&) = delete;

	// Store the borrowed texture both for GraphExecutor and RenderGraph.
	// This enables GraphExecutor to connect the logical operation by RenderGraph to the actual backend operation.
	Result ImportTexture(
		const GraphTextureBinding& texture,
		const std::string& name,
		GraphTextureHandle& output);

	Result Execute();

private:
	struct StateCommit
	{
		ID3D12Resource* resource = nullptr;
		D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
	};

	RenderGraph& m_graph;
	backend::Backend& m_backend;
	std::vector<GraphTextureBinding> m_bindings;

private:
	[[nodiscard]] const GraphTextureBinding* FindBinding(GraphTextureHandle handle) const noexcept;

	// Final check for compiled logical execution plan from RenderGraph.
	// Compares the logical plan with the actual backend state of the borrowed texture resources.
	Result Preflight(const CompiledPlan& plan, std::vector<StateCommit>& commits) const;

	Result RecordTransitions(const std::vector<PlannedTransition>& transitions);
};

}