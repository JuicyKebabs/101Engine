#pragma once

#include "Renderer/FrameRenderData.h"
#include "Graph/GraphExecutor.h"

namespace rendering
{

class FrameRenderer
{
public:
	Result Build(
		const FrameRenderData& frame,
		backend::Backend& backend,
		graph::RenderGraph& graph,
		graph::GraphExecutor& executor) const;
};

}