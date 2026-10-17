#include "GeometryCpuData.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>

namespace rendering::detail
{

namespace
{

Result ValidateGeometryDesc(const GeometryDesc& desc)
{
	if (desc.topology != PrimitiveTopology::TriangleList)
	{
		return { false, "Unsupported primitive topology." };
	}

	if (desc.vertices.empty())
	{
		return { false, "Vertex array is empty." };
	}

	// One 32-bit-sized view per vertex / index buffer.
	constexpr std::size_t maxBufferBytes = (std::numeric_limits<std::uint32_t>::max)();

	if (desc.vertices.size() > maxBufferBytes / sizeof(MeshVertex))
	{
		return { false, "Vertex array is too large." };
	}

	if (desc.indices.size() > maxBufferBytes / sizeof(uint32_t))
	{
		return { false, "Index array is too large." };
	}

	for (const auto& vertex : desc.vertices)
	{
		const float values[]{
			vertex.position.x, vertex.position.y, vertex.position.z,
			vertex.normal.x, vertex.normal.y, vertex.normal.z,
			vertex.uv.x, vertex.uv.y,
			vertex.tangent.x, vertex.tangent.y, vertex.tangent.z,
			vertex.color.x, vertex.color.y, vertex.color.z, vertex.color.w
		};

		for (const float value : values)
		{
			if (!std::isfinite(value))
			{
				return { false, "Geometry has a non-finite vertex attribute." };
			}
		}
	}

	for (const std::uint32_t index : desc.indices)
	{
		if (index >= desc.vertices.size())
		{
			return { false, "Geometry index is out of range." };
		}
	}

	return {};
}

}

Result CopyGeometryCpuData(const GeometryDesc& desc, GeometryCpuData& output)
{
	Result validation = ValidateGeometryDesc(desc);
	if (!validation.IsSuccess())
	{
		return validation;
	}

	try
	{
		GeometryCpuData copied;
		copied.topology = desc.topology;
		copied.vertices.assign(desc.vertices.begin(), desc.vertices.end());

		if (!desc.indices.empty())
		{
			copied.indices.assign(desc.indices.begin(), desc.indices.end());
		}

		// Commit only after the copy to temporary storage is successful.
		output.vertices.swap(copied.vertices);
		output.indices.swap(copied.indices);
		output.topology = copied.topology;
		return {};
	}
	catch (const std::bad_alloc&)
	{
		return { false, "Geometry CPU allocation failed." };
	}
	catch (const std::length_error&)
	{
		return { false, "Geometry CPU array size is unsupported." };
	}
}

}