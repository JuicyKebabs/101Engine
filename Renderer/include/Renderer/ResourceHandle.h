// R7 implementation guide: section 4.1.
// Logical identity only: owner, slot and generation reject cross-Renderer and stale references.
// RenderTargetTexture has its own tag; it is never a CPU-upload Texture with a usage flag.
// Copying a handle does not retain a Content revision or extend GPU object lifetime.

#pragma once

#include <cstdint>
#include <limits>

//---------------------------------------------------------------------------------
// ResourceHandle class and specific resource handle type aliases
// Template class to represent a handle for various rendering resources
// and type aliases for specific resource handles (GeometryHandle, TextureHandle).
//---------------------------------------------------------------------------------

namespace rendering
{

namespace detail
{

struct GeometryTag;
struct TextureTag;
struct RenderTargetTextureTag;
struct ShaderTag;
struct PipelineTag;
struct MaterialTag;

template <class Tag>
struct ResourceHandle
{
	static constexpr std::uint32_t InvalidIndex = (std::numeric_limits<uint32_t>::max)();

	uint32_t ownerId = InvalidIndex;		// The ID for the renderer instance that owns this resource handle.
	uint32_t slotIndex = InvalidIndex;		// The index indicating the location in the resource array for given resource type.
	uint32_t generation = InvalidIndex;		// The number of reusing times for the slot index when this handle is created, used to detect stale handles.

	constexpr bool operator==(const ResourceHandle& other) const noexcept = default;
};

}

// Type aliases for specific resource handles.
// Avoid confusion with other resource handles at compile time.
using GeometryHandle = detail::ResourceHandle<detail::GeometryTag>;
using TextureHandle = detail::ResourceHandle<detail::TextureTag>;
// Different tag means different public type, even though the existing owner/slot/generation mechanism is shared.
// The handle remains stable on Resize and becomes stale after Destroy plus slot reuse.
using RenderTargetTextureHandle = detail::ResourceHandle<detail::RenderTargetTextureTag>;
using ShaderHandle = detail::ResourceHandle<detail::ShaderTag>;
using PipelineHandle = detail::ResourceHandle<detail::PipelineTag>;
using MaterialHandle = detail::ResourceHandle<detail::MaterialTag>;

// Static assertions to ensure the size of the resource handles is as expected.
static_assert(sizeof(GeometryHandle) == 12);
static_assert(sizeof(TextureHandle) == 12);
static_assert(sizeof(RenderTargetTextureHandle) == 12);
static_assert(sizeof(ShaderHandle) == 12);
static_assert(sizeof(PipelineHandle) == 12);
static_assert(sizeof(MaterialHandle) == 12);

}
