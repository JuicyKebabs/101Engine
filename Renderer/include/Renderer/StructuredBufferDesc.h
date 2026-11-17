#pragma once

#include <cstdint>

//---------------------------------------------------------------------
// StructuredBufferDesc
// Description struct for creating structured buffers with public API.
//---------------------------------------------------------------------

namespace rendering
{

struct StructuredBufferDesc
{
	std::uint32_t elementCount = 0;		// Number of elements in the structured buffer.
	std::uint32_t elementStride = 0;	// Bytes per element, matching the shader's layout.
};

}