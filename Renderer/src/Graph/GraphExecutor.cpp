#include "GraphExecutor.h"
#include "RenderPassContext.h"

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace rendering::graph
{

namespace
{

Result DescribeTexture(const GraphTextureBinding& texture, ImportedTextureDesc& desc)
{
	ID3D12Resource* resource = texture.Resource();

	if (!resource || !texture.Rtv().ptr)
	{
		return { false, "DescribeTexture: native texture or RTV is unavailable." };
	}

	// Resource description validation
	const auto native = resource->GetDesc();

	if (native.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
		|| native.Width == 0 || native.Height == 0 ||
		native.Width > static_cast<UINT64>(D3D12_VIEWPORT_BOUNDS_MAX) ||
		native.Height > static_cast<UINT>(D3D12_VIEWPORT_BOUNDS_MAX) ||
		native.DepthOrArraySize != 1 || native.MipLevels != 1 ||
		native.SampleDesc.Count != 1 || native.SampleDesc.Quality != 0 ||
		!(native.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ||
		(native.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) ||
		native.Format != texture.ViewFormat())
	{
		return { false, "DescribeTexture: unsupported native texture extent, flags, format or sample conditions." };
	}

	switch (texture.ViewFormat())
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		desc.format = GraphTextureFormat::RGBA8_UNorm; break;

	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		desc.format = GraphTextureFormat::RGBA8_sRGB; break;

	default:
		return { false, "unsupported native RTV format; D32/DSV execution is deferred." };
	}

	desc.physical = PhysicalTextureId{ resource };
	desc.width = static_cast<std::uint32_t>(native.Width);
	desc.height = native.Height;
	desc.capabilities = TextureCapability::ColorAttachment;

	// Present available. (backbuffer)
	if (texture.IsPresentable())
	{
		desc.capabilities = desc.capabilities | TextureCapability::Present;
	}

	const auto state = texture.SubmittedState();

	if (state == D3D12_RESOURCE_STATE_COMMON)
	{
		// Force initial state of backbuffer to be present.
		desc.initialState = { texture.IsPresentable() ? TextureStateKind::Present : TextureStateKind::Common };
	}
	else if (state == D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		// Render target has to be color attachment state.
		desc.initialState = { TextureStateKind::ColorAttachment };
	}
	else
	{
		return { false, "unsupported native initial state." };
	}

	desc.initialContentsValid = false;

	return {};
}

Result ToNativeState(TextureState state, D3D12_RESOURCE_STATES& output)
{
	output = D3D12_RESOURCE_STATE_COMMON;

	// Only shader read can require shader stages.
	if (state.kind != TextureStateKind::ShaderRead && state.stages != ShaderStage::None)
	{
		return { false, "invalid stages on a non-shader state." };
	}

	switch (state.kind)
	{
	case TextureStateKind::Common:

	case TextureStateKind::Present:
		return {};

	case TextureStateKind::ColorAttachment:
		output = D3D12_RESOURCE_STATE_RENDER_TARGET;
		return {};

	case TextureStateKind::DepthWrite:
		return { false, "D32/DSV GPU recording is not supported yet." };

	case TextureStateKind::ShaderRead:
		if (!IsValidStages(state.stages))
		{
			return { false, "invalid shader stages." };
		}

		if ((static_cast<unsigned>(state.stages) & static_cast<unsigned>(ShaderStage::Vertex)) != 0)
		{
			output |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}

		if ((static_cast<unsigned>(state.stages) & static_cast<unsigned>(ShaderStage::Pixel)) != 0)
		{
			output |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}

		return {};

	default:
		return { false, "unknown texture state." };
	}
}

}

const GraphTextureBinding* GraphExecutor::FindBinding(GraphTextureHandle handle) const noexcept
{
	const auto found = std::find_if(
		m_bindings.begin(), m_bindings.end(),
		[handle](const GraphTextureBinding& binding) { return binding.handle == handle; });

	return found == m_bindings.end() ? nullptr : &*found;
}

Result GraphExecutor::ImportTexture(
	const GraphTextureBinding& texture,
	const std::string& name,
	GraphTextureHandle& output)
{
	output = {};

	Result result = m_graph.CheckMutable();

	if (!result.IsSuccess())
	{
		return result;
	}

	m_graph.m_plan.reset();

	// Get the latest state of the texture resouce from backend.
	backend::BackBufferInfo owner;
	Result refreshed = m_backend.QueryTextureInfo(texture.Resource(), owner);

	if (!refreshed.IsSuccess())
	{
		return refreshed;
	}

	// Compare the latest state of the texture resource with the borrowed view/state.
	if (owner.IsPresentable() != texture.IsPresentable() || 
		owner.Rtv().ptr != texture.Rtv().ptr || 
		owner.ViewFormat() != texture.ViewFormat() ||
		owner.SubmittedState() != texture.SubmittedState())
	{
		return { false, "GraphExecutor::ImportTexture: stale or mismatched borrowed view/state." };
	}

	try
	{
		ImportedTextureDesc desc;	// The way for graph to hold the texture.
		desc.name = name;

		// Generate the description of the texture resource for the graph to hold.
		result = DescribeTexture(texture, desc);

		if (!result.IsSuccess())
		{
			return result;
		}

		// Store the borrowed texture for this frame execution (both for executor and graph)
		m_bindings.push_back(texture);					// Store the borrowed texture for this frame execution.
		result = m_graph.ImportTexture(desc, output);	// Add the texture to the graph and get a handle for it.

		if (!result.IsSuccess())
		{
			m_bindings.pop_back();
			return result;
		}

		// Store the handle in the binding for later use.
		m_bindings.back().handle = output;

		return {};
	}
	catch (const std::bad_alloc&) 
	{
		return { false, "GraphExecutor::ImportTexture: allocation failed." };
	}
	catch (const std::length_error&)
	{ 
		return { false, "GraphExecutor::ImportTexture: size is unsupported." };
	}
}

Result GraphExecutor::Preflight(const CompiledPlan& plan, std::vector<StateCommit>& commits) const
{
	for (const auto& pass : plan.passes)
	{
		if (pass.depth || pass.colors.size() != 1)
		{
			return { false, "GraphExecutor: GPU execution currently requires one color and no Depth." };
		}
	}

	// Un-binded texture is not allowed.
	if (plan.textures.size() != m_bindings.size())
	{
		return { false, "GraphExecutor: imported texture has no native binding." };
	}

	commits.reserve(plan.textures.size());

	for (const auto& final : plan.textures)
	{
		// Checl if both executor and graph know this texture.
		const auto* binding = FindBinding(final.texture);
		const auto* imported = m_graph.FindTexture(final.texture);

		if (!binding || !imported)
		{
			return { false, "GraphExecutor: missing, stale or foreign native binding." };
		}

		// Check the mismatch between resource and it's binding.
		backend::BackBufferInfo owner;
		Result checked = m_backend.QueryTextureInfo(binding->Resource(), owner);

		if (!checked.IsSuccess())
		{
			return checked;
		}

		if (owner.IsPresentable() != binding->IsPresentable() || 
			owner.Rtv().ptr != binding->Rtv().ptr || 
			owner.ViewFormat() != binding->ViewFormat())
		{
			return { false, "GraphExecutor: owner View changed after Import." };
		}

		// Check if the resource got changed after imported.
		GraphTextureBinding current = *binding;
		current.initialState = owner.SubmittedState();

		ImportedTextureDesc actual;
		checked = DescribeTexture(current, actual);

		if (!checked.IsSuccess())
		{
			return checked;
		}

		if (actual.physical != imported->physical || actual.format != imported->format ||
			actual.dimension != imported->dimension || actual.mipLevels != imported->mipLevels ||
			actual.arraySize != imported->arraySize ||
			actual.width != imported->width || actual.height != imported->height ||
			actual.sampleCount != imported->sampleCount || actual.sampleQuality != imported->sampleQuality ||
			actual.capabilities != imported->capabilities || actual.initialState != imported->initialState ||
			actual.initialContentsValid != imported->initialContentsValid)
		{
			return { false, "GraphExecutor: native texture conditions changed after Import." };
		}

		// Presentable resource has to be presentable at the end.
		if (binding->IsPresentable() && final.state.kind != TextureStateKind::Present)
		{
			return { false, "GraphExecutor: presentable texture must finish in Present state." };
		}

		// Store the final state of this resource.
		// Stored state will be reflected after execution is finished.
		D3D12_RESOURCE_STATES state{};
		checked = ToNativeState(final.state, state);

		if (!checked.IsSuccess())
		{
			return checked;
		}

		commits.push_back({ binding->Resource(), state });
	}

	// Verify that the output layout of this each pass match the conditions of the connected render target texture.
	for (const auto& pass : plan.passes)
	{
		const auto* binding = FindBinding(pass.colors.front().texture);
		const auto* imported = m_graph.FindTexture(pass.colors.front().texture);

		if (!binding || !imported || 
			pass.outputLayout.depthFormat ||
			pass.outputLayout.colorFormats.size() != 1 ||
			pass.outputLayout.colorFormats.front() != imported->format ||
			pass.outputLayout.sampleCount != imported->sampleCount ||
			pass.outputLayout.sampleQuality != imported->sampleQuality)
		{
			return { false, "GraphExecutor: OutputLayout does not match the actual attachments." };
		}
	}

	return {};
}

Result GraphExecutor::RecordTransitions(const std::vector<PlannedTransition>& transitions)
{
	auto& commands = m_backend.GetFrameCommandContext();

	for (const auto& transition : transitions)
	{
		const auto* binding = FindBinding(transition.texture);

		if (!binding)
		{
			return { false, "GraphExecutor: transition has no native binding." };
		}

		D3D12_RESOURCE_STATES before{}, after{};

		Result result = ToNativeState(transition.before, before);

		if (!result.IsSuccess())
		{
			return result;
		}

		result = ToNativeState(transition.after, after);

		if (!result.IsSuccess())
		{
			return result;
		}

		result = commands.TransitionResource(binding->Resource(), before, after);

		if (!result.IsSuccess())
		{
			return result;
		}
	}

	return {};
}

Result GraphExecutor::Execute()
{
	if (m_graph.m_executionStarted)
	{
		if (m_graph.m_executing)
		{
			m_graph.m_executionViolated = true;
		}

		return { false, "GraphExecutor: this Graph has already attempted execution." };
	}

	if (!m_graph.m_plan)
	{
		return { false, "GraphExecutor: no valid compiled plan." };
	}

	m_graph.m_executionStarted = true;
	m_graph.m_executing = true;

	struct ExecutionScope
	{
		bool& running;
		~ExecutionScope() { running = false; }
	};
	
	ExecutionScope scope = { m_graph.m_executing };

	const auto& plan = *m_graph.m_plan;
	std::string passName;

	try
	{
		std::vector<StateCommit> commits;
		Result result = Preflight(plan, commits);

		if (!result.IsSuccess())
		{
			return result;
		}

		// View-less frames may have independent uploads, but require no graphics list.
		if (plan.passes.empty() && plan.finalTransitions.empty())
		{
			return {};
		}

		for (const auto& binding : m_bindings)
		{
			result = m_backend.KeepFrameObjectAlive(binding.Resource());

			if (!result.IsSuccess())
			{
				return result;
			}
		}

		result = m_backend.BeginFrameCommands();

		if (!result.IsSuccess())
		{
			return result;
		}

		auto& commands = m_backend.GetFrameCommandContext();

		for (const auto& compiled : plan.passes)
		{
			const auto& source = m_graph.m_passes[compiled.sourcePass];
			passName = source.Description().name;

			const auto fail =
				[&](Result status) -> Result
				{
					status.message = "GraphExecutor Pass '" + passName + "': " + status.message;
					return status;
				};

			result = RecordTransitions(compiled.transitions);

			if (!result.IsSuccess())
			{
				return fail(std::move(result));
			}

			const auto& attachment = compiled.colors.front();
			const auto* binding = FindBinding(attachment.texture);
			const auto* desc = m_graph.FindTexture(attachment.texture);
			const std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 1> rtvs{ binding->Rtv() };

			result = commands.SetRasterOutput(rtvs, desc->width, desc->height);

			if (!result.IsSuccess())
			{
				return fail(std::move(result));
			}

			if (attachment.loadOp == AttachmentLoadOp::Clear)
			{
				result = commands.ClearRenderTarget(rtvs.front(), attachment.clearColor);

				if (!result.IsSuccess())
				{
					return fail(std::move(result));
				}
			}

			RenderPassContext context(m_graph, compiled)
;
			result = source.m_execute(context);

			if (!context.m_status.IsSuccess())
			{
				return fail(std::move(context.m_status));
			}

			if (m_graph.m_executionViolated)
			{
				return fail({ false, "Graph mutation or recursive execution was attempted." });
			}

			if (!result.IsSuccess())
			{
				return fail(std::move(result));
			}
		}

		passName.clear();
		result = RecordTransitions(plan.finalTransitions);

		if (!result.IsSuccess())
		{
			return result;
		}

		result = commands.Close();

		if (!result.IsSuccess())
		{
			return result;
		}

		const auto submission = m_backend.SubmitFrameCommands();

		if (!submission.status.IsSuccess())
		{
			return submission.status;
		}

		if (!submission.executionIssued)
		{
			return { false, "GraphExecutor: successful submission issued no execution." };
		}

		// All identities and states are prepared. No allocation or validation after Signal.
		for (const auto& commit : commits)
		{
			m_backend.CommitTextureState(commit.resource, commit.state);
		}

		return m_backend.WaitForFrameCompletion();
	}
	catch (const std::bad_alloc&)
	{
		return { false, "GraphExecutor: allocation failed during recording." };
	}
	catch (const std::length_error&)
	{ 
		return { false, "GraphExecutor: recording size is unsupported." };
	}
	catch (const std::exception& error)
	{
		return { false, "GraphExecutor Pass '" + passName + "': callback/recording exception: " + error.what() };
	}
	catch (...) 
	{
		return { false, "GraphExecutor: unknown callback/recording exception." };
	}
}

}