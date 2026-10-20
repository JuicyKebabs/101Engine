#include "RenderGraph.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace rendering::graph
{

namespace
{

	std::uint64_t ReserveGraphId() noexcept
	{
		static std::atomic<std::uint64_t> next{ 1 };

		auto value = next.load(std::memory_order_relaxed);

		while (value != (std::numeric_limits<std::uint64_t>::max)())
		{
			if (next.compare_exchange_weak(value, value + 1, std::memory_order_relaxed)) return value;
		}

		return 0;
	}

	Result ValidateUse(const ImportedTextureDesc& desc, TextureUse use)
	{
		if (use.usage == TextureUsage::ShaderRead)
		{
			if (!IsValidStages(use.stages))
			{
				return { false, "ValidateUse: ShaderRead requires supported Vertex/Pixel stages." };
			}
		}
		else if (use.stages != ShaderStage::None)
		{
			return { false, "ValidateUse: only ShaderRead may specify stages." };
		}

		// As to D32_Float, depth attachment requires it, but color attachment and shader read do not support it.
		// For each TextureUsage, check if the imported texture has a capability that supports it.
		switch (use.usage)
		{
		case TextureUsage::ShaderRead:
			if (desc.format == GraphTextureFormat::D32_Float ||
				!HasCapability(desc.capabilities, TextureCapability::ShaderRead))
			{
				return { false, "ShaderRead is unsupported by the texture format/capabilities." };
			}

			break;

		case TextureUsage::ColorAttachment:
			if (desc.format == GraphTextureFormat::D32_Float ||
				!HasCapability(desc.capabilities, TextureCapability::ColorAttachment))
			{
				return { false, "ColorAttachment is unsupported by the texture format/capabilities." };
			}

			break;

		case TextureUsage::DepthAttachment:
			if (desc.format != GraphTextureFormat::D32_Float ||
				!HasCapability(desc.capabilities, TextureCapability::DepthAttachment))
			{
				return { false, "DepthAttachment requires D32_Float with depth capability." };
			}

			break;

		case TextureUsage::Present:
			if (!HasCapability(desc.capabilities, TextureCapability::Present))
			{
				return { false, "Present requires a presentable back buffer." };
			}

			break;

		default:
			return { false, "unsupported texture usage." };
		}

		return {};
	}

	TextureState RequiredState(TextureUse use) noexcept
	{
		switch (use.usage)
		{
		case TextureUsage::ShaderRead:
			return { TextureStateKind::ShaderRead, use.stages };

		case TextureUsage::ColorAttachment:
			return { TextureStateKind::ColorAttachment };

		case TextureUsage::DepthAttachment:
			return { TextureStateKind::DepthWrite };

		case TextureUsage::Present:
			return { TextureStateKind::Present };

		default:
			return {}; // Callers validate first; an unsupported use never reaches here.
		}
	}

	Result ValidateImport(const ImportedTextureDesc& desc)
	{
		// Required data existance.
		if (desc.name.empty() || !desc.physical.value)
		{
			return { false, "ValidateImport: name is empty or physical texture is null." };
		}

		// Valid texture settings.
		if (desc.dimension != TextureDimension::Texture2D || !desc.width || !desc.height ||
			desc.mipLevels != 1 || desc.arraySize != 1 || desc.sampleCount != 1 || desc.sampleQuality != 0)
		{
			return { false, "ValidateImport: unsupported texture settings." };
		}

		// Supported texture formats.
		switch (desc.format)
		{
		case GraphTextureFormat::RGBA8_UNorm:
		case GraphTextureFormat::RGBA8_sRGB:
		case GraphTextureFormat::D32_Float:
			break;
		default:
			return { false, "ValidateImport: unsupported texture format." };
		}

		// Supported texture capabilities.
		const auto caps = static_cast<unsigned>(desc.capabilities);

		if (!caps || (caps & ~15u))
		{
			return { false, "ValidateImport: unsupported texture capabilities." };
		}

		// Validated texture usage for each capability.
		for (const auto pair : {
			std::pair{ TextureCapability::ShaderRead, TextureUsage::ShaderRead },
			std::pair{ TextureCapability::ColorAttachment, TextureUsage::ColorAttachment },
			std::pair{ TextureCapability::DepthAttachment, TextureUsage::DepthAttachment } })
		{
			if (HasCapability(desc.capabilities, pair.first))
			{
				const auto stages = pair.second == TextureUsage::ShaderRead ? ShaderStage::Pixel : ShaderStage::None;
				Result result = ValidateUse(desc, { pair.second, stages });

				if (!result.IsSuccess())
				{
					return result;
				}
			}
		}

		// Back buffer requirements.
		if (HasCapability(desc.capabilities, TextureCapability::Present) &&
			(desc.format != GraphTextureFormat::RGBA8_UNorm ||
				!HasCapability(desc.capabilities, TextureCapability::ColorAttachment) ||
				desc.initialState.kind != TextureStateKind::Present || desc.initialContentsValid))
		{
			return { false, "ValidateImport: back buffer requires RGBA8_UNorm, color capability, Present state and invalid initial contents." };
		}

		// Validated initial state for ShaderRead (No shader stage is not accepted).
		if (desc.initialState.kind != TextureStateKind::ShaderRead && desc.initialState.stages != ShaderStage::None)
		{
			return { false, "ValidateImport: only ShaderRead state may specify stages." };
		}

		// Validate initial state for each kind.
		switch (desc.initialState.kind)
		{
		case TextureStateKind::Common:
			return {};

		case TextureStateKind::Present:
			return ValidateUse(desc, { TextureUsage::Present });

		case TextureStateKind::ShaderRead:
			return ValidateUse(desc, { TextureUsage::ShaderRead, desc.initialState.stages });

		case TextureStateKind::ColorAttachment:
			return ValidateUse(desc, { TextureUsage::ColorAttachment });

		case TextureStateKind::DepthWrite:
			return ValidateUse(desc, { TextureUsage::DepthAttachment });

		default:
			return { false, "ValidateImport: unsupported initial texture state." };
		}
	}

}

RenderGraph::RenderGraph() noexcept : m_id(ReserveGraphId()) {}

const ImportedTextureDesc* RenderGraph::FindTexture(GraphTextureHandle texture) const noexcept
{
	if (!m_id || texture.graphId != m_id || texture.index >= m_resources.size())
	{
		return nullptr;
	}

	return &m_resources[texture.index].desc;
}

Result RenderGraph::ImportTexture(const ImportedTextureDesc& desc, GraphTextureHandle& output)
{
	m_plan.reset();
	output = {};

	if (!m_id)
	{
		return { false, "ImportTexture: Graph IDs are exhausted." };
	}

	try
	{
		Result result = ValidateImport(desc);

		if (!result.IsSuccess())
		{
			return { false, "ImportTexture: '" + desc.name + "'  " + result.message };
		}

		// Check for duplicate physical texture.
		for (const auto& resource : m_resources)
		{
			if (resource.desc.physical == desc.physical)
			{
				return { false, "ImportTexture: '" + desc.name + "' physical texture is already imported as '" + resource.desc.name + "'." };
			}
		}

		if (m_resources.size() >= GraphTextureHandle::InvalidIndex)
		{
			return { false, "ImportTexture: too many imported textures." };
		}

		// Store the imported texture description. Final usage is not set yet.
		m_resources.push_back({ desc, std::nullopt });

		const auto index = static_cast<std::uint32_t>(m_resources.size() - 1);
		output = { m_id, index };

		return {};
	}
	catch (const std::bad_alloc&)
	{
		return { false, "ImportTexture: allocation failed." };
	}
	catch (const std::length_error&)
	{
		return { false, "ImportTexture: too many imported textures." };
	}
}

Result RenderGraph::SetFinalUsage(GraphTextureHandle texture, TextureUse use)
{
	m_plan.reset();

	const auto* desc = FindTexture(texture);

	if (!desc)
	{
		return { false, "SetFinalUsage: handle is invalid or belongs to another Graph." };
	}

	Result result = ValidateUse(*desc, use);

	if (!result.IsSuccess())
	{
		return { false, "SetFinalUsage: '" + desc->name + "'  " + result.message };
	}

	m_resources[texture.index].finalUse = use;

	return {};
}

Result RenderGraph::AddPass(RenderPass&& pass)
{
	m_plan.reset();

	if (!m_id)
	{
		return { false, "AddPass: Graph IDs are exhausted." };
	}

	if (!pass.m_valid || !pass.m_execute)
	{
		return { false, "AddPass: pass is empty or already moved." };
	}

	if (m_passes.size() >= (std::numeric_limits<std::uint32_t>::max)())
	{
		return { false, "AddPass: pass index space is exhausted." };
	}

	try
	{
		m_passes.push_back(std::move(pass));
		return {};
	}
	catch (const std::bad_alloc&)
	{
		return { false, "AddPass: allocation failed." };
	}
	catch (const std::length_error&)
	{
		return { false, "AddPass: too many passes." };
	}
}

Result RenderGraph::Compile()
{
	m_plan.reset();

	if (!m_id)
	{
		return { false, "Compile: Graph IDs are exhausted." };
	}

	try
	{
		CompiledPlan candidate;

		std::vector<TextureState> states;
		std::vector<bool> contentsValid;

		states.reserve(m_resources.size());
		contentsValid.reserve(m_resources.size());

		for (const auto& resource : m_resources)
		{
			if (!resource.finalUse)
			{
				return { false, "Compile: Texture '" + resource.desc.name + "': final usage is missing." };
			}

			states.push_back(resource.desc.initialState);
			contentsValid.push_back(resource.desc.initialContentsValid);
		}

		// Compile every pass in order
		for (std::size_t passIndex = 0; passIndex < m_passes.size(); ++passIndex)
		{
			const auto& source = m_passes[passIndex].Description();

			// Lambds to return result with failure message including the pass name and index.
			const auto fail = [&](GraphTextureHandle texture, const std::string& reason) -> Result
				{
					const auto* desc = FindTexture(texture);
					const std::string name = desc ? desc->name : "<unresolved>";

					return { false, "Compile: Pass[" + std::to_string(passIndex) + "] '" + source.name +
						"', Texture[" + std::to_string(texture.index) + "] '" + name + "': " + reason };
				};

			CompiledPass compiled;
			compiled.sourcePass = static_cast<std::uint32_t>(passIndex);
			compiled.colors = source.colors;
			compiled.depth = source.depth;

			for (const auto& use : source.uses)
			{
				const auto* desc = FindTexture(use.texture);

				if (!desc)
				{
					return fail(use.texture, "handle is invalid or belongs to another Graph.");
				}

				Result checked = ValidateUse(*desc, use.use);

				if (!checked.IsSuccess())
				{
					return fail(use.texture, checked.message);
				}

				auto previous = std::find_if(
					compiled.uses.begin(), compiled.uses.end(),
					[&](const PassTextureUse& item) { return item.texture == use.texture; });

				if (previous == compiled.uses.end())
				{
					compiled.uses.push_back(use);
				}
				else if (previous->use.usage == TextureUsage::ShaderRead && use.use.usage == TextureUsage::ShaderRead)
				{
					previous->use.stages = previous->use.stages | use.use.stages;
				}
				else
				{
					return fail(use.texture, "conflicting Read/Write or duplicate attachment in the same pass.");
				}
			}

			const ImportedTextureDesc* firstAttachment = nullptr;

			const auto checkAttachment = [&](GraphTextureHandle texture)->Result
				{
					const auto* desc = FindTexture(texture);

					if (!desc)
					{
						return fail(texture, "handle is invalid or belongs to another Graph.");
					}

					if (firstAttachment)
					{
						bool isDifferent = desc->width != firstAttachment->width || desc->height != firstAttachment->height ||
							desc->sampleCount != firstAttachment->sampleCount || desc->sampleQuality != firstAttachment->sampleQuality;

						if (isDifferent)
						{
							return fail(texture, "attachment size or sample settings differ from the first attachment.");
						}
					}

					firstAttachment = desc;
					return {};
				};

			for (const auto& color : compiled.colors)
			{
				Result checked = checkAttachment(color.texture);

				if (!checked.IsSuccess())
				{
					return checked;
				}
			}

			if (compiled.depth)
			{
				Result checked = checkAttachment(compiled.depth->texture);

				if (!checked.IsSuccess())
				{
					return checked;
				}
			}

			for (const auto& use : compiled.uses)
			{
				const auto index = use.texture.index;

				if (use.requiresValidContents && !contentsValid[index])
				{
					return fail(use.texture, "Read/Load requires valid contents; no valid initial contents or preceding Clear.");
				}

				const auto required = RequiredState(use.use);

				if (states[index] != required)
				{
					compiled.transitions.push_back({ use.texture, states[index], required });
				}

				states[index] = required;

				if (use.initializesContents)
				{
					contentsValid[index] = true;
				}
			}

			candidate.passes.push_back(std::move(compiled));
		}

		for (std::size_t index = 0; index < m_resources.size(); ++index)
		{
			const auto& resource = m_resources[index];
			const auto finalUse = *resource.finalUse;

			Result checked = ValidateUse(resource.desc, finalUse);

			if (!checked.IsSuccess())
			{
				return { false, "Compile final usage '" + resource.desc.name + "': " + checked.message };
			}

			if ((finalUse.usage == TextureUsage::ShaderRead || finalUse.usage == TextureUsage::Present) && !contentsValid[index])
			{
				return { false, "Compile final usage: Texture[" + std::to_string(index) + "] '" + resource.desc.name + "': ShaderRead/Present requires valid contents." };
			}

			const GraphTextureHandle texture{ m_id, static_cast<std::uint32_t>(index) };

			const auto required = RequiredState(finalUse);

			if (states[index] != required)
			{
				candidate.finalTransitions.push_back({ texture, states[index], required });
			}

			candidate.textures.push_back({ texture, required, contentsValid[index] });
		}

		m_plan.emplace(std::move(candidate));

		return {};
	}
	catch (const std::bad_alloc&)
	{
		return { false, "Compile: allocation failed." };
	}
	catch (const std::length_error&)
	{
		return { false, "Compile: too many passes or resources." };
	}

}

}