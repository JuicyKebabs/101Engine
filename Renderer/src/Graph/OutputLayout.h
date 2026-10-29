#pragma once

#include "GraphResource.h"
#include <optional>
#include <vector>

//--------------------------------------------------------
// OutputLayout
// PSO compability information for a compiled reder pass.
//--------------------------------------------------------

namespace rendering::graph
{

struct OutputLayout
{
	std::vector<GraphTextureFormat> colorFormats;
	std::optional<GraphTextureFormat> depthFormat;
	std::uint32_t sampleCount = 1;
	std::uint32_t sampleQuality = 0;
	bool operator==(const OutputLayout& other) const = default;
};

}