#include "GeometryCpuData.h"

#include <cmath>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>

namespace rendering::detail
{

namespace
{

// A conservative sphere around the local-space AABB also encloses every
// indexed/unindexed vertex. Use double intermediates to avoid float addition
// overflow when individually finite positions are close to FLT_MAX.
Result CalculateBounds(GeometryCpuData& data)
{
    std::array<double, 3> minimum;
    std::array<double, 3> maximum;
    minimum.fill((std::numeric_limits<double>::max)());
    maximum.fill((std::numeric_limits<double>::lowest)());

    for (const MeshVertex& vertex : data.vertices)
    {
        const std::array<double, 3> position{ vertex.position.x, vertex.position.y, vertex.position.z };
        for (std::size_t axis = 0; axis < position.size(); ++axis)
        {
            minimum[axis] = (std::min)(minimum[axis], position[axis]);
            maximum[axis] = (std::max)(maximum[axis], position[axis]);
        }
    }

    Vector3 center;
    center.x = static_cast<float>((minimum[0] + maximum[0]) * 0.5);
    center.y = static_cast<float>((minimum[1] + maximum[1]) * 0.5);
    center.z = static_cast<float>((minimum[2] + maximum[2]) * 0.5);

    // Calculate from the rounded center that the caller will actually receive.
    // This includes the center's float conversion error in the radius.
    double radius = 0;
    for (const MeshVertex& vertex : data.vertices)
    {
        const double horizontal = static_cast<double>(vertex.position.x) - center.x;
        const double vertical = static_cast<double>(vertex.position.y) - center.y;
        const double depth = static_cast<double>(vertex.position.z) - center.z;
        const double distance = std::hypot(horizontal, vertical, depth);
        radius = (std::max)(radius, distance);
    }

    float roundedRadius = static_cast<float>(radius);
    if (static_cast<double>(roundedRadius) < radius)
    {
        roundedRadius = std::nextafter(roundedRadius, (std::numeric_limits<float>::infinity)());
    }

    if (!std::isfinite(roundedRadius))
    {
        Result failure;
        failure.succeeded = false;
        failure.message = "Geometry local Bounds cannot be represented by a finite float sphere.";
        return failure;
    }

    data.boundsCenter = center;
    data.boundsRadius = roundedRadius;
    return {};
}

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

        Result boundsResult = CalculateBounds(copied);
        if (!boundsResult.IsSuccess())
        {
            return boundsResult;
        }

        // Commit only after CPU copy and matching Bounds are both complete.
        output.boundsCenter = copied.boundsCenter;
        output.boundsRadius = copied.boundsRadius;
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