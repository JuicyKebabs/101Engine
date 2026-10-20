#pragma once

#include "GraphResource.h"
#include "Renderer/Result.h"

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace rendering::graph
{

class RenderPassContext;
class RenderGraph;

using ExecuteCallback = std::function<Result(RenderPassContext&)>;

enum class AttachmentLoadOp 
{ 
	Clear,
	Load
};

// Attachment descriptions are used in the actual rendering process.

struct ColorAttachment
{
	std::uint32_t slot = 0;
	GraphTextureHandle texture;
	AttachmentLoadOp loadOp = AttachmentLoadOp::Clear;
	std::array<float, 4> clearColor{};
};

struct DepthAttachment
{
	GraphTextureHandle texture;
	AttachmentLoadOp loadOp = AttachmentLoadOp::Clear;
	float clearDepth = 1.0f;
};

// Used for validating the texture usage in a render pass,
// Created from attachment descriptions automatically.
struct PassTextureUse
{
	GraphTextureHandle texture;
	TextureUse use;
	bool requiresValidContents = false;	// If this pass requires previous contents. (LoadOp::Clear == false, LoadOp::Load == true)
	bool initializesContents = false;	// If this pass initializes contents. (LoadOp::Clear == true, LoadOp::Load == false)
};

struct PassDescription
{
	std::string name;
	std::vector<PassTextureUse> uses;
	std::vector<ColorAttachment> colors;
	std::optional<DepthAttachment> depth;
};

class RenderPass
{
public:
	RenderPass(const RenderPass&) = delete;
	RenderPass& operator=(const RenderPass&) = delete;
	RenderPass(RenderPass&& other) noexcept;
	RenderPass& operator=(RenderPass&& other) noexcept;

	[[nodiscard]] const PassDescription& Description() const noexcept { return m_description; }

private:
	RenderPass(PassDescription description, ExecuteCallback execute) noexcept;

	PassDescription m_description;
	ExecuteCallback m_execute;
	bool m_valid = true;

	friend class RenderPassBuilder;
	friend class RenderGraph;
};

class RenderPassBuilder
{
public:
	RenderPassBuilder() = default;
	RenderPassBuilder(const RenderPassBuilder&) = delete;
	RenderPassBuilder& operator=(const RenderPassBuilder&) = delete;
	RenderPassBuilder(RenderPassBuilder&&) = delete;
	RenderPassBuilder& operator=(RenderPassBuilder&&) = delete;

	Result SetName(const std::string& name);
	Result ReadTexture(GraphTextureHandle texture, ShaderStage stages);
	Result ColorOutput(ColorAttachment attachment);
	Result DepthOutput(DepthAttachment attachment);
	Result SetExecute(ExecuteCallback execute);

	// One-shot operation, including failure. Output is empty when failure.
	Result Build(std::optional<RenderPass>& output)&&;

private:
	// Only single description and execution is allowed per builder (also per pass).
	PassDescription m_description;
	ExecuteCallback m_execute;
	bool m_consumed = false;
};

}