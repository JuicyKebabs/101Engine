#include "FrameRenderer.h"
#include "Graph/RenderPassContext.h"

#include <new>
#include <stdexcept>
#include <utility>

namespace rendering
{

Result FrameRenderer::Build(
	const FrameRenderData& frame,
	backend::Backend& backend,
	graph::RenderGraph& graph,
	graph::GraphExecutor& executor) const
{
	if (frame.views.size() > 1)
	{
		return { false, "FrameRenderer: only one View is supported." };
	}

	try
	{
		for (const auto& view : frame.views)
		{
			backend::BackBufferInfo output;
			Result result = backend.GetCurrentWindowTexture(view.output, output);

			if (!result.IsSuccess())
			{
				return result;
			}

			graph::GraphTextureHandle backBuffer;
			const graph::GraphTextureBinding binding{
				output.Resource(),
				output.Rtv(),
				output.ViewFormat(),
				output.SubmittedState(),
				output.IsPresentable()
			};

			result = executor.ImportTexture(binding, "BackBuffer", backBuffer);

			if (!result.IsSuccess())
			{
				return result;
			}

			result = graph.SetFinalUsage(backBuffer, { graph::TextureUsage::Present });

			if (!result.IsSuccess())
			{
				return result;
			}

			graph::RenderPassBuilder builder;

			result = builder.SetName("ClearWindowOutput");

			if (!result.IsSuccess())
			{
				return result;
			}

			graph::ColorAttachment attachment = {
				0,
				backBuffer,
				graph::AttachmentLoadOp::Clear,
				{ 0.05f, 0.10f, 0.25f, 1.0f }
			};

			result = builder.ColorOutput(attachment);

			if (!result.IsSuccess())
			{
				return result;
			}

			// Clear belongs to attachment preparation, before this callback.
			result = builder.SetExecute([](graph::RenderPassContext&) -> Result { return {}; });

			if (!result.IsSuccess())
			{
				return result;
			}

			std::optional<graph::RenderPass> pass;

			result = std::move(builder).Build(pass);

			if (!result.IsSuccess())
			{
				return result;
			}

			result = graph.AddPass(std::move(*pass));

			if (!result.IsSuccess())
			{
				return result;
			}
		}

		return {};
	}
	catch (const std::bad_alloc&)
	{
		return { false, "FrameRenderer: allocation failed." };
	}
	catch (const std::length_error&)
	{
		return { false, "FrameRenderer: size is unsupported." };
	}
}

}