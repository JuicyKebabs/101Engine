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
using GeometryHandle = detail::ResourceHandle<detail::GeometryTag>;
using TextureHandle = detail::ResourceHandle<detail::TextureTag>;

// Static assertions to ensure the size of the resource handles is as expected.
static_assert(sizeof(GeometryHandle) == 12);
static_assert(sizeof(TextureHandle) == 12);

}