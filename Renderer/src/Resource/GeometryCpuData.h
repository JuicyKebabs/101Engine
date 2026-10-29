#pragma once

#include "Renderer/Geometry.h"
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
    // Calculated once while accepting a complete CPU copy. Revision is added by
    // the registry, so Bounds and Revision can never describe different contents.
    Vector3 boundsCenter;
    float boundsRadius = 0;
};

[[nodiscard]]
Result CopyGeometryCpuData(const GeometryDesc& desc, GeometryCpuData& output);

}