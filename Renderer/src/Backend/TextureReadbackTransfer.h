#pragma once

#include "Buffer.h"
#include "Renderer/TextureReadback.h"

//---------------------------------------------------------------------
// TextureReadbackTransfer
// Contains the necessary information for a texture readback operation.
//---------------------------------------------------------------------

namespace rendering::backend
{

class Backend;
class CommandContext;

class TextureReadbackTransfer
{
public:
	std::size_t RowBytes() const noexcept { return m_rowBytes; }
	std::size_t PackedBytes() const noexcept { return m_rowBytes * m_region.height; }
	std::size_t ReservedBytes() const noexcept { return m_reservedBytes; }
	bool HasBuffer() const noexcept { return m_buffer.GetSize() != 0; }

	// The caller has confirmed GPU completion or discarded the unsubmitted commands.
	void ReleaseBuffer() noexcept { m_buffer.Finalize(); }

private:
	Buffer m_buffer;
	PixelRect m_region;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_footprint{};
	std::uint64_t m_transferBytes = 0;
	std::size_t m_rowBytes = 0;
	std::size_t m_reservedBytes = 0;

	friend class Backend;
	friend class CommandContext;
};

}