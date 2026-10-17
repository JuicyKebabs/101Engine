#pragma once

#include "Renderer/TextureDesc.h"
#include "Renderer/Result.h"
#include <vector>

//-------------------------------------------------------------------
// TextureCpuData
// Structure to hold texture data in CPU memory for transfer to GPU.
//-------------------------------------------------------------------

namespace rendering::detail
{

struct TextureMipCpuData
{
	std::vector<std::byte> pixels;
	std::size_t rowPitch = 0;
};

struct TextureCpuData
{
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	TextureFormat format = TextureFormat::RGBA8_UNorm;
	std::vector<TextureMipCpuData> mips;
};

[[nodiscard]] Result CopyTextureCpuData(const TextureDesc& desc, TextureCpuData& output);

}