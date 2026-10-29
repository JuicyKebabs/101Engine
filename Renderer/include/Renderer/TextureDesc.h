#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

//--------------------------------------------------------------------------
// Texture description
// Used to register texture resources via Register() API from user code.
// Mip levels are stored in two-dimensional array format by TextureMipDesc.
//--------------------------------------------------------------------------

namespace rendering
{

enum class TextureFormat
{
	RGBA8_UNorm,
	RGBA8_sRGB,
    // Generated color outputs only; CPU upload remains RGBA8.
    RGBA16_Float,
};

struct TextureMipDesc
{
	// Non-owning view of the pixel data for this mip level.
	std::span<const std::byte> pixels{};

	// Byte distance between the starts of adjacent CPU rows.
	std::size_t rowPitch = 0;
};

struct TextureDesc
{
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	TextureFormat format = TextureFormat::RGBA8_UNorm;
	std::span<const TextureMipDesc> mips{};
};

}