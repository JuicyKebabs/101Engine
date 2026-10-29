// R7 implementation guide: section 4.18.
// Declare exact resource identities before Compile and reuse them during draw recording.
// Render targets can be both attachments and sampled inputs across different passes.
// Their Depth remains output-specific and is omitted from Overlay passes.

#include "FrameResourceBindings.h"
#include "Backend/DepthTexture.h"
#include "Resource/TextureRegistry.h"
#include "Resource/RenderTargetTextureRegistry.h"
#include "Failure.h"

#include <algorithm>

namespace rendering::graph
{

FrameResourceBindings::FrameResourceBindings(
	backend::Backend& backend,
	RenderGraph& graph,
	GraphExecutor& executor) noexcept
	: m_backend(backend), m_graph(graph), m_executor(executor)
{}

// Reuse existing physical identity rather than creating independent aliases for read and write.
// Otherwise Graph could miss the dependency between a Scene output and a later Material sample.
Result FrameResourceBindings::ImportBinding(
	const GraphTextureBinding& binding,
	const std::string& name,
	GraphTextureHandle& texture)
{
	// ponytail: linear lookup suits small Graphs; index by native Resource if profiling shows a bottleneck.
	const auto existing = std::find_if(
		m_importedBindings.begin(), m_importedBindings.end(),
		[&binding](const GraphTextureBinding& imported)
		{
			return imported.Resource() == binding.Resource();
		});

	if (existing != m_importedBindings.end())
	{
		texture = existing->handle;
		return {};
	}

	// Allocate our entry before changing the Graph/Executor. Import failure rolls it back.
	m_importedBindings.push_back(binding);
	GraphTextureHandle importedTexture;
	Result result = m_executor.ImportTexture(binding, name, importedTexture);

	if (!result.IsSuccess())
	{
		m_importedBindings.pop_back();
		return result;
	}

	m_importedBindings.back().handle = importedTexture;
	texture = importedTexture;
	return {};
}

Result FrameResourceBindings::ImportWindowOutput(
	WindowOutputHandle output,
	GraphTextureHandle& texture,
	ViewTargets& targets)
{
	backend::BackBufferInfo backBuffer;
	Result result = m_backend.GetCurrentWindowTexture(output, backBuffer);

	if (!result.IsSuccess())
	{
		return result;
	}

	const auto description = backBuffer.Resource()->GetDesc();
	targets.width = static_cast<std::uint32_t>(description.Width);
	targets.height = description.Height;

	GraphTextureBinding binding;
	binding.resource = backBuffer.Resource();
	binding.rtv = backBuffer.Rtv();
	binding.viewFormat = backBuffer.ViewFormat();
	binding.initialState = backBuffer.SubmittedState();
	binding.presentable = true;
	return ImportBinding(binding, "BackBuffer", texture);
}

Result FrameResourceBindings::ImportDepth(backend::DepthTexture& depth, GraphTextureHandle& texture)
{
	GraphTextureBinding binding;
	binding.resource = depth.Resource();
	binding.dsv = depth.Dsv();
	binding.viewFormat = DXGI_FORMAT_D32_FLOAT;
	binding.initialState = depth.submittedState;
	binding.contentsValid = depth.contentsValid;
	binding.owner = &depth;
	return ImportBinding(binding, "SceneDepth", texture);
}

Result FrameResourceBindings::PrepareViewTargets(
	WindowOutputHandle output,
	backend::DepthTexture& depth,
	ViewTargets& targets,
	bool includeDepth)
{
	ViewTargets preparedTargets;
	Result result = ImportWindowOutput(output, preparedTargets.color, preparedTargets);

	if (!result.IsSuccess())
	{
		return result;
	}

	if (includeDepth)
	{
		result = depth.Ensure(m_backend.GetNativeDevice(), preparedTargets.width, preparedTargets.height);
		if (!result.IsSuccess())
		{
			auto* device = m_backend.GetNativeDevice();
			if (device && FAILED(device->GetDeviceRemovedReason()))
			{
				result.failureKind = FailureKind::DeviceLost;
			}
			return result;
		}

		result = ImportDepth(depth, preparedTargets.depth);
		if (!result.IsSuccess())
		{
			return result;
		}

		TextureUse finalDepthUse;
		finalDepthUse.usage = TextureUsage::DepthAttachment;
		result = m_graph.SetFinalUsage(preparedTargets.depth, finalDepthUse);
		if (!result.IsSuccess())
		{
			return result;
		}
	}

	TextureUse finalColorUse;
	finalColorUse.usage = TextureUsage::Present;
	result = m_graph.SetFinalUsage(preparedTargets.color, finalColorUse);

	if (!result.IsSuccess())
	{
		return result;
	}

	targets = preparedTargets;
	return {};
}

// Import the color and, for Scene only, its already-matching private Depth.
// Overlay does not import Depth. No allocation/replacement is performed after a resource is imported.
Result FrameResourceBindings::PrepareRenderTargetTargets(
	detail::RenderTargetTextureContent& content,
	bool includeDepth,
	ViewTargets& targets)
{
	if (!content.gpuData)
	{
		return detail::Fail("FrameResourceBindings: RenderTargetTexture has no GPU output.");
	}

	ViewTargets preparedTargets;
	preparedTargets.width = content.gpuData->width;
	preparedTargets.height = content.gpuData->height;
	Result imported = ImportRenderTarget(content, "SceneOutput", preparedTargets.color);
	if (!imported.IsSuccess())
	{
		return imported;
	}

	if (includeDepth)
	{
		// Matching Depth was created atomically with the color image, before Import.
		imported = ImportDepth(content.gpuData->Depth(), preparedTargets.depth);
		if (!imported.IsSuccess())
		{
			return imported;
		}

		TextureUse finalDepthUse;
		finalDepthUse.usage = TextureUsage::DepthAttachment;
		imported = m_graph.SetFinalUsage(preparedTargets.depth, finalDepthUse);
		if (!imported.IsSuccess())
		{
			return imported;
		}
	}

	targets = preparedTargets;
	return {};
}

// Both an attachment writer and a Material reader reach this same import.
// Keep the final color usage shader-readable in both Vertex and Pixel stages for later consumption.
// State transition alone does not authorize reading uninitialized contents.
Result FrameResourceBindings::ImportRenderTarget(
	detail::RenderTargetTextureContent& content,
	const std::string& name,
	GraphTextureHandle& texture)
{
	if (!content.gpuData || !content.gpuData->Resource())
	{
		return detail::Fail("FrameResourceBindings: RenderTargetTexture is unavailable.");
	}

	auto& gpu = *content.gpuData;
	GraphTextureBinding binding;
	binding.resource = gpu.Resource();
	binding.rtv = gpu.Rtv();
	binding.srv = gpu.Srv();
	binding.viewFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	binding.initialState = gpu.submittedState;
	binding.contentsValid = gpu.contentsValid;
	binding.owner = &content;
	Result imported = ImportBinding(binding, name, texture);
	if (!imported.IsSuccess())
	{
		return imported;
	}

	TextureUse finalUse;
	finalUse.usage = TextureUsage::ShaderRead;
	finalUse.stages = ShaderStage::Vertex | ShaderStage::Pixel;
	return m_graph.SetFinalUsage(texture, finalUse);
}

// Recording may only look up resources already declared before Compile.
// A missing entry is a contract error, not a reason to import a surprise dependency during execution.
Result FrameResourceBindings::FindImportedRenderTarget(
	const detail::RenderTargetTextureContent& content,
	GraphTextureHandle& texture) const
{
	const auto existing = std::find_if(
		m_importedBindings.begin(), m_importedBindings.end(),
		[&content](const GraphTextureBinding& binding)
		{
			const auto owner = std::get_if<detail::RenderTargetTextureContent*>(&binding.owner);
			return owner && *owner == &content;
		});
	if (existing == m_importedBindings.end())
	{
		return detail::Fail("FrameResourceBindings: RenderTargetTexture was not declared before execution.");
	}

	texture = existing->handle;
	return {};
}

Result FrameResourceBindings::ImportTexture(
	detail::TextureContent& content,
	const std::string& name,
	GraphTextureHandle& texture)
{
	if (content.state.preparationState != detail::ResourcePreparationState::Ready || !content.gpuData)
	{
		return { false, "FrameResourceBindings: Texture is not ready." };
	}

	auto& gpu = *content.gpuData;
	GraphTextureBinding binding;
	binding.resource = gpu.texture.Get();
	binding.srv = gpu.GetSrvHandle();
	binding.viewFormat = gpu.texture->GetDesc().Format;
	binding.initialState = gpu.submittedState;
	binding.contentsValid = gpu.contentsValid;
	binding.owner = &content;
	Result result = ImportBinding(binding, name, texture);

	if (!result.IsSuccess())
	{
		return result;
	}

	TextureUse finalTextureUse;
	finalTextureUse.usage = TextureUsage::ShaderRead;
	finalTextureUse.stages = ShaderStage::Vertex | ShaderStage::Pixel;
	return m_graph.SetFinalUsage(texture, finalTextureUse);
}

Result FrameResourceBindings::FindImportedTexture(
	const detail::TextureContent& content,
	GraphTextureHandle& texture) const
{
	const auto existing = std::find_if(
		m_importedBindings.begin(), m_importedBindings.end(),
		[&content](const GraphTextureBinding& binding)
		{
			const auto owner = std::get_if<detail::TextureContent*>(&binding.owner);
			return owner && *owner == &content;
		});

	if (existing == m_importedBindings.end())
	{
		return { false, "FrameResourceBindings: Texture was not declared before execution." };
	}

	texture = existing->handle;
	return {};
}

}
