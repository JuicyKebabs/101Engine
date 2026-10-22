#include "RenderPass.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <stdexcept>
#include <utility>

namespace rendering::graph
{

namespace
{

bool IsValidLoadOp(AttachmentLoadOp op) noexcept
{
	return op == AttachmentLoadOp::Clear || op == AttachmentLoadOp::Load;
}

}

RenderPass::RenderPass(PassDescription description, ExecuteCallback execute) noexcept
	: m_description(std::move(description)), m_execute(std::move(execute))
{}

RenderPass::RenderPass(RenderPass&& other) noexcept
	: m_description(std::move(other.m_description)),
	m_execute(std::move(other.m_execute)),
	m_valid(std::exchange(other.m_valid, false))
{}

RenderPass& RenderPass::operator=(RenderPass&& other) noexcept
{
	if (this != &other)
	{
		m_description = std::move(other.m_description);
		m_execute = std::move(other.m_execute);
		m_valid = std::exchange(other.m_valid, false);
	}

	return *this;
}

Result RenderPassBuilder::SetName(const std::string& name)
{
	if (m_consumed)
	{
		return { false, "Builder: already consumed." };
	}

	try 
	{ 
		m_description.name = name; 
		return {}; 
	}
	catch (const std::bad_alloc&)
	{
		return { false, "Builder: name allocation failed." };
	}
	catch (const std::length_error&)
	{
		return { false, "Builder: name is too long." };
	}
}

Result RenderPassBuilder::ReadTexture(GraphTextureHandle texture, ShaderStage stages)
{
	if (m_consumed)
	{
		return { false, "Builder: already consumed." };
	}

	try
	{
		PassTextureUse textureUse = {};
		textureUse.texture = texture;
		textureUse.use = { TextureUsage::ShaderRead, stages };
		textureUse.requiresValidContents = true;
		textureUse.initializesContents = false;

		m_description.uses.push_back(textureUse);
		return {};
	}
	catch (const std::bad_alloc&)
	{
		return { false, "Builder: color allocation failed." };
	}
	catch (const std::length_error&)
	{
		return { false, "Builder: too many color attachments." };
	}
}

Result RenderPassBuilder::ColorOutput(ColorAttachment attachment)
{
	if (m_consumed)
	{
		return { false, "Builder: already consumed." };
	}

	try
	{
		m_description.colors.push_back(attachment); 
		return {};
	}
	catch (const std::bad_alloc&)
	{ 
		return { false, "Builder: color allocation failed." }; 
	}
	catch (const std::length_error&) 
	{ 
		return { false, "Builder: too many color attachments." }; 
	}
}

Result RenderPassBuilder::DepthOutput(DepthAttachment attachment)
{
	if (m_consumed)
	{
		return { false, "Builder: already consumed." };
	}

	if (m_description.depth)
	{
		return { false, "Builder: depth attachment is already set." };
	}

	m_description.depth = attachment;
	return {};
}

Result RenderPassBuilder::SetExecute(ExecuteCallback execute)
{
	if (m_consumed)
	{
		return { false, "Builder: already consumed." };
	}

	m_execute = std::move(execute);
	return {};
}

Result RenderPassBuilder::Build(std::optional<RenderPass>& output)&&
{
	output.reset();

	// false to true transition is required (means only one build is allowed per builder).
	if (std::exchange(m_consumed, true))
	{
		return { false, "Build: Builder was already consumed." };
	}

	try
	{
		// Lambda to return result with failure message including the pass name.
		const auto fail = [&](const std::string& reason) -> Result
			{
				return { false, "Build: Pass '" + m_description.name + "': " + reason };
			};

		if (m_description.name.empty())
		{
			return fail("name is empty.");
		}

		if (!m_execute)
		{
			return fail("Execute callback is empty.");
		}

		if (m_description.colors.empty() && !m_description.depth)
		{

			return fail("Raster requires an attachment.");
		}

		if (m_description.colors.size() > 1)
		{
			return fail("only one color attachment is supported.");
		}

		for (const auto& read : m_description.uses)
		{
			if (!IsValid(read.texture))
			{
				return fail("Read has an invalid Graph handle.");
			}

			if (!IsValidStages(read.use.stages))
			{
				return fail("Read requires supported Vertex/Pixel stages.");
			}
		}

		for (const auto& color : m_description.colors)
		{
			if (color.slot != 0)
			{
				return fail("only color slot 0 is supported.");
			}

			if (!IsValid(color.texture) || !IsValidLoadOp(color.loadOp))
			{
				return fail("invalid color attachment.");
			}

			// Check that all clear color values are finite (not NaN or Inf).
			bool isAllFinite = std::all_of(
				color.clearColor.begin(), color.clearColor.end(),
				[](float v) { return std::isfinite(v); });

			if (color.loadOp == AttachmentLoadOp::Clear && !isAllFinite)
			{
				return fail("color Clear must contain finite values.");
			}

			if (m_description.depth && m_description.depth->texture == color.texture)
			{
				return fail("the same texture cannot be both color and depth.");
			}
		}

		if (m_description.depth)
		{
			const auto& depth = *m_description.depth;

			if (!IsValid(depth.texture) || !IsValidLoadOp(depth.loadOp))
			{
				return fail("invalid depth attachment.");
			}

			bool isDepthValid =
				std::isfinite(depth.clearDepth)
				&& depth.clearDepth >= 0.0f
				&& depth.clearDepth <= 1.0f;

			if (depth.loadOp == AttachmentLoadOp::Clear && !isDepthValid)
			{
				return fail("depth Clear must be finite and in [0, 1].");
			}
		}

		// Generates the usage of the attachments for the pass description.
		for (const auto& color : m_description.colors)
		{
			const bool clear = color.loadOp == AttachmentLoadOp::Clear;
			
			PassTextureUse textureUse = {};
			textureUse.texture = color.texture;
			textureUse.use = { TextureUsage::ColorAttachment, ShaderStage::None };
			textureUse.requiresValidContents = !clear;
			textureUse.initializesContents = clear;

			m_description.uses.push_back(std::move(textureUse));
		}

		if (m_description.depth)
		{
			const bool clear = m_description.depth->loadOp == AttachmentLoadOp::Clear;

			PassTextureUse textureUse = {};
			textureUse.texture = m_description.depth->texture;
			textureUse.use = { TextureUsage::DepthAttachment, ShaderStage::None };
			textureUse.requiresValidContents = !clear;
			textureUse.initializesContents = clear;

			m_description.uses.push_back(std::move(textureUse));
		}

		// Generate the final RenderPass object and return it in the output parameter.
		RenderPass pass(std::move(m_description), std::move(m_execute));
		output.emplace(std::move(pass));

		return {};
	}
	catch (const std::bad_alloc&)
	{
		return { false, "Build: allocation failed." };
	}
	catch (const std::length_error&)
	{
		return { false, "Build: description size is unsupported." };
	}
}

}