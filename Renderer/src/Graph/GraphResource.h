#pragma once

#include <cstdint>
#include <limits>
#include <string>

//------------------------------------------------------------------------------
// RenderGraph Resource Identifiers and Descriptions
// Graph-local resource descriptions and handles used to validate pass usages
// and plan state transitions, separate from Renderer-managed resource handles.
//------------------------------------------------------------------------------

namespace rendering::graph
{

// Identifier for a texture resource used in RenderGraph.
struct GraphTextureHandle
{
	static constexpr std::uint64_t InvalidIndex = (std::numeric_limits<std::uint64_t>::max)();

	std::uint64_t graphId = 0;
	std::uint64_t index = InvalidIndex;
	constexpr bool operator==(const GraphTextureHandle&) const noexcept = default;
};

[[nodiscard]] constexpr bool IsValid(GraphTextureHandle handle) noexcept
{
	return handle.graphId != 0 && handle.index != GraphTextureHandle::InvalidIndex;
}

// Store the address of the resource.
// Used to identify the physical texture resource (for avoiding duplicated imports)
struct PhysicalTextureId
{
	const void* value = nullptr;
	constexpr bool operator==(const PhysicalTextureId&) const noexcept = default;
};

enum class ShaderStage : std::uint8_t 
{ 
	None = 0,
	Vertex = 1,
	Pixel = 2 
};

[[nodiscard]] constexpr ShaderStage operator|(ShaderStage a, ShaderStage b) noexcept
{
	return static_cast<ShaderStage>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

[[nodiscard]] constexpr bool IsValidStages(ShaderStage stages) noexcept
{
	const auto bits = static_cast<unsigned>(stages);
	return bits != 0 && (bits & ~3u) == 0;
}

enum class GraphTextureFormat
{
	RGBA8_UNorm,
	RGBA8_sRGB,
    RGBA16_Float,
	D32_Float
};

enum class TextureDimension
{
	Texture2D
};

enum class TextureCapability : std::uint8_t
{
	None = 0,
	ShaderRead = 1,
	ColorAttachment = 2,
	DepthAttachment = 4,
	Present = 8
};

[[nodiscard]] constexpr TextureCapability operator|(TextureCapability a, TextureCapability b) noexcept
{
	return static_cast<TextureCapability>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

[[nodiscard]] constexpr bool HasCapability(TextureCapability value, TextureCapability required) noexcept
{
	return (static_cast<unsigned>(value) & static_cast<unsigned>(required)) == static_cast<unsigned>(required);
}

enum class TextureUsage
{
	ShaderRead,
	ColorAttachment,
	DepthAttachment,
	Present
};

struct TextureUse
{
	TextureUsage usage = TextureUsage::ShaderRead;
	ShaderStage stages = ShaderStage::None;
};

enum class TextureStateKind
{
	Common,
	Present,
	ShaderRead,
	ColorAttachment,
	DepthWrite,
};

struct TextureState
{
	TextureStateKind kind = TextureStateKind::Common;
	ShaderStage stages = ShaderStage::None;
	constexpr bool operator==(const TextureState&) const noexcept = default;
};

// Used for validating render pass which uses the imported texture resource.
struct ImportedTextureDesc
{
	std::string name;
	PhysicalTextureId physical;
	TextureDimension dimension = TextureDimension::Texture2D;
	GraphTextureFormat format = GraphTextureFormat::RGBA8_UNorm;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint32_t mipLevels = 1;
	std::uint32_t arraySize = 1;
	std::uint32_t sampleCount = 1;
	std::uint32_t sampleQuality = 0;
	TextureCapability capabilities = TextureCapability::None;
	TextureState initialState;
	bool initialContentsValid = false;
};

}