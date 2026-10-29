#pragma once

#include "Buffer.h"
#include "CommandContext.h"
#include "Resource/GeometryCpuData.h"

#include <memory>

namespace rendering::backend
{

struct GeometryGpuData
{
	Buffer vertices;
	Buffer indices;
	D3D12_VERTEX_BUFFER_VIEW vertexView{};
	D3D12_INDEX_BUFFER_VIEW indexView{};
	std::uint32_t vertexCount = 0;
	std::uint32_t indexCount = 0;
};

struct GeometryUploadData
{
	Buffer vertices;
	Buffer indices;
};

[[nodiscard]]
Result PrepareGeometryUpload(
	ID3D12Device* device,
	const detail::GeometryCpuData& cpuData,
	std::unique_ptr<GeometryGpuData>& gpuOutput,
	std::unique_ptr<GeometryUploadData>& uploadOutput);

[[nodiscard]]
Result RecordGeometryUpload(
	CommandContext& context,
	const GeometryGpuData& gpuData,
	const GeometryUploadData& uploadData);

}