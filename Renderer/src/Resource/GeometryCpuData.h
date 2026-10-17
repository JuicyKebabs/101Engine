#pragma once

#include "Renderer/GeometryDesc.h"
#include "Renderer/Result.h"

#include <vector>

//--------------------------------------------------------------------
// GeometryCpuData
// Structure to hold geometry data in CPU memory for transfer to GPU.
//--------------------------------------------------------------------

namespace rendering::detail
{

struct GeometryCpuData
{
	std::vector<MeshVertex> vertices;
	std::vector<uint32_t> indices;
	PrimitiveTopology topology = PrimitiveTopology::TriangleList;
};

[[nodiscard]]
Result CopyGeometryCpuData(const GeometryDesc& desc, GeometryCpuData& output);

}