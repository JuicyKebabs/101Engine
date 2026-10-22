#include "RenderPassContext.h"

#include <algorithm>

namespace rendering::graph
{

Result RenderPassContext::Reject(const char* reason)
{
	// Set yourself up for failure.
	if (m_status.IsSuccess())
	{
		m_status = { false, reason };
	}

	return m_status;
}

Result RenderPassContext::GetTextureInfo(GraphTextureHandle texture, TextureUse intendedUse, PassTextureInfo& output)
{
	output = {};

	if (!m_status.IsSuccess())
	{
		return m_status;
	}

	// Get description of required resource from graph.
	const auto* desc = m_graph.FindTexture(texture);

	if (!desc)
	{
		return Reject("Context: invalid texture or different Graph.");
	}

	// Get usage of the texture in this pass.
	const auto found = std::find_if(
		m_pass.uses.begin(), m_pass.uses.end(),
		[texture](const PassTextureUse& item) { return item.texture == texture; });

	if (found == m_pass.uses.end())
	{
		return Reject("Context: texture is not used in this pass.");
	}

	// Requirements for the intended usage must match the declared usage in the compiled pass.
	if (found->use.usage != intendedUse.usage)
	{
		return Reject("Context: requested usage differs from this Pass declaration.");
	}

	// Va;idate required usage.
	if (intendedUse.usage == TextureUsage::ShaderRead)
	{
		const auto requested = static_cast<unsigned>(intendedUse.stages);
		const auto declared = static_cast<unsigned>(found->use.stages);

		if (!IsValidStages(intendedUse.stages) || (requested & ~declared) != 0)
		{
			return Reject("Context: requested shader stages were not declared.");
		}
	}
	else if (intendedUse.stages != ShaderStage::None || intendedUse.usage == TextureUsage::Present)
	{
		return Reject("Context: invalid Pass usage or shader stages.");
	}

	output = { desc->format, desc->width, desc->height, desc->sampleCount, desc->sampleQuality };
	return {};
}


}