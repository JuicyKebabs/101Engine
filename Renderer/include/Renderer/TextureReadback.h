#pragma once

#include "ResourceHandle.h"
#include "Result.h"
#include "TextureDesc.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rendering
{

enum class ReadbackTiming
{
	BeforeRendering,
	AfterRendering
};

enum class ReadbackState
{
	Pending,
	Ready,
	Failed
};

struct PixelRect
{
	std::uint32_t x = 0;
	std::uint32_t y = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

struct TextureReadbackDesc
{
	RenderTargetTextureHandle sourceTexture;
	std::uint64_t expectedSourceRevision = 0;
	PixelRect region;
	ReadbackTiming timing = ReadbackTiming::AfterRendering;
};

// Used to return the CPU result of a completed readback request.
struct TextureReadbackData
{
	TextureFormat format = TextureFormat::RGBA8_UNorm;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::size_t rowPitch = 0;
	std::vector<std::byte> pixels;
};

struct [[nodiscard]] ReadbackRequestResult
{
	Result status;
	ReadbackHandle handle;
};


struct [[nodiscard]] ReadbackPollResult
{
	ReadbackState state = ReadbackState::Pending;
	Result error;
};

}