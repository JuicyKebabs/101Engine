#include "TextureCpuData.h"

#include <bit>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <algorithm>

namespace rendering::detail
{

namespace
{

constexpr std::size_t BytesPerPixel = 4; // Assuming RGBA8 format for simplicity

Result ValidateTextureDesc(const TextureDesc& desc)
{
	if (desc.format != TextureFormat::RGBA8_UNorm && desc.format != TextureFormat::RGBA8_sRGB)
	{
		return { false, "Unsupported texture format." };
	}

	if (desc.width == 0 || desc.height == 0)
	{
		return { false, "Texture dimensions must be greater than zero." };
	}

	const auto largestDimension = std::max(desc.width, desc.height);
	const auto maxMipCount = std::bit_width(largestDimension);

	if (desc.mips.empty() || desc.mips.size() > maxMipCount)
	{
		return { false, "Invalid number of mip levels." };
	}

	constexpr auto maxSize = (std::numeric_limits<std::size_t>::max)();

	std::size_t width = desc.width;
	std::size_t height = desc.height;

	for (const auto& mip : desc.mips)
	{
		if (width > maxSize / BytesPerPixel)
		{
			return { false, "Texture row size overflows." };
		}

		const std::size_t rowBytes = width * BytesPerPixel;

		if (mip.rowPitch < rowBytes)
		{
			return { false, "Row pitch is smaller than the row size." };
		}

		if (height > maxSize / rowBytes)
		{
			return { false, "Texture CPU mip size overflows." };
		}

		// The final row does not need trailing padding.
		if (height - 1 > (maxSize - rowBytes) / mip.rowPitch)
		{
			return { false, "Texture source range overflows." };
		}

		const std::size_t requiredBytes = (height - 1) * mip.rowPitch + rowBytes;

		if (mip.pixels.size() < requiredBytes)
		{
			return { false, "Texture mip pixel data is too small." };
		}

		width = std::max<std::size_t>(1, width / 2);
		height = std::max<std::size_t>(1, height / 2);
	}

	return {};
}

}

Result CopyTextureCpuData(const TextureDesc& desc, TextureCpuData& output)
{
	const Result validation = ValidateTextureDesc(desc);

	if (!validation.IsSuccess())
	{
		return validation;
	}

	try
	{
		// Copy data to temporal structure.
		TextureCpuData copied;
		copied.width = desc.width;
		copied.height = desc.height;
		copied.format = desc.format;
		copied.mips.resize(desc.mips.size());

		std::size_t width = desc.width;
		std::size_t height = desc.height;

		// Copy each mip level's pixel data to the destination structure.
		for (std::size_t level = 0; level < desc.mips.size(); ++level)
		{
			const auto& source = desc.mips[level];
			auto& destination = copied.mips[level];
			const std::size_t rowBytes = width * BytesPerPixel;

			destination.rowPitch = rowBytes;
			destination.pixels.resize(rowBytes * height);

			for (std::size_t row = 0; row < height; ++row)
			{
				const std::byte* sourceRow = source.pixels.data() + row * source.rowPitch;
				std::byte* destinationRow = destination.pixels.data() + row * destination.rowPitch;
				std::memcpy(destinationRow, sourceRow, rowBytes);
			}

			width = std::max<std::size_t>(1, width / 2);
			height = std::max<std::size_t>(1, height / 2);
		}

		// Commit only after every mips has been copied successfully.
		output.mips.swap(copied.mips);
		output.width = copied.width;
		output.height = copied.height;
		output.format = copied.format;

		return {};
	}
	catch (const std::bad_alloc&)
	{
		return { false, "Texture CPU data allocation failed." };
	}
	catch (const std::length_error&)
	{
		return { false, "Texture CPU data size is unsupported." };
	}
}

}