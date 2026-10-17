#include "GeometryGpuData.h"

#include <limits>
#include <new>
#include <utility>

namespace rendering::backend
{

Result PrepareGeometryUpload(
	ID3D12Device* device,
	const detail::GeometryCpuData& cpuData,
	std::unique_ptr<GeometryGpuData>& gpuOutput,
	std::unique_ptr<GeometryUploadData>& uploadOutput)
{
	if (!device || gpuOutput || uploadOutput)
	{
		return { false, "Geometry upload requires a device and empty outputs." };
	}

	constexpr auto maxBytes = (std::numeric_limits<std::uint32_t>::max)();

	// Validate the CPU geometry data
	if (cpuData.topology != PrimitiveTopology::TriangleList ||
		cpuData.vertices.empty() ||
		cpuData.vertices.size() > maxBytes / sizeof(MeshVertex) ||
		cpuData.indices.size() > maxBytes / sizeof(std::uint32_t))
	{
		return  { false, "Geometry CPU data is not supported." };
	}

	try
	{
		auto gpuData = std::make_unique<GeometryGpuData>();
		auto uploadData = std::make_unique<GeometryUploadData>();

		// Helper lambda to prepare a buffer for upload and GPU usage
		const auto prepareBuffer = 
			[&](std::span<const std::byte> bytes, Buffer& destination, Buffer& source) -> Result
			{
				Result result = destination.Initialize(device, bytes.size(), BufferMemory::Default);
				if (!result.IsSuccess())
				{
					return result;
				}

				result = source.Initialize(device, bytes.size(), BufferMemory::Upload);
				if (!result.IsSuccess())
				{
					return result;
				}

				return source.Write(bytes);
			};

		// Prepare vertex buffer
		Result result = prepareBuffer(
			std::as_bytes(std::span{ cpuData.vertices }),
			gpuData->vertices,
			uploadData->vertices);

		if (!result.IsSuccess())
		{
			return result;
		}

		gpuData->vertexCount = static_cast<std::uint32_t>(cpuData.vertices.size());
		gpuData->vertexView.BufferLocation = gpuData->vertices.GetNativeResource()->GetGPUVirtualAddress();
		gpuData->vertexView.SizeInBytes = static_cast<UINT>(gpuData->vertices.GetSize());
		gpuData->vertexView.StrideInBytes = static_cast<UINT>(sizeof(MeshVertex));

		// Prepare index buffer if indices are provided
		if (!cpuData.indices.empty())
		{
			result = prepareBuffer(
				std::as_bytes(std::span{ cpuData.indices }),
				gpuData->indices,
				uploadData->indices);

			if (!result.IsSuccess())
			{
				return result;
			}

			gpuData->indexCount = static_cast<std::uint32_t>(cpuData.indices.size());
			gpuData->indexView.BufferLocation = gpuData->indices.GetNativeResource()->GetGPUVirtualAddress();
			gpuData->indexView.SizeInBytes = static_cast<UINT>(gpuData->indices.GetSize());
			gpuData->indexView.Format = DXGI_FORMAT_R32_UINT;
		}

		// Move to the output after successful preparation
		gpuOutput = std::move(gpuData);
		uploadOutput = std::move(uploadData);
		return {};
	}
	catch (const std::bad_alloc&)
	{
		return { false, "Geometry upload allocation failed." };
	}
};

Result RecordGeometryUpload(
	CommandContext& context,
	const GeometryGpuData& gpuData,
	const GeometryUploadData& uploadData)
{
	const auto validPair = [](const Buffer& destination, const Buffer& source) -> bool
		{
			bool validResources = destination.GetNativeResource() && source.GetNativeResource();
			bool validSizes = destination.GetSize() == source.GetSize() && destination.GetSize() > 0;
			return validResources && validSizes;
		};

	if (!context.IsRecording())
	{
		return { false, "RecordGeometryUpload: CommandContext is not recording." };
	}

	if (!validPair(gpuData.vertices, uploadData.vertices))
	{
		return { false, "RecordGeometryUpload: Invalid vertex buffer pair." };
	}

	const bool indexed = gpuData.indexCount != 0;
	const bool validIndexPair = validPair(gpuData.indices, uploadData.indices);
	const bool indexBuffersPresent = gpuData.indices.GetNativeResource() || uploadData.indices.GetNativeResource();
	
	if ((indexed && !validIndexPair) || (!indexed && indexBuffersPresent))
	{
		return { false, "Geometry index upload buffers are inconsistent." };
	}

	const auto recordBuffer =
		[&](const Buffer& destination, const Buffer& source, D3D12_RESOURCE_STATES finalState)->Result
		{
			Result result = context.CopyBufferRegion(
				destination.GetNativeResource(), 0,
				source.GetNativeResource(), 0,
				destination.GetSize());

			if (!result.IsSuccess())
			{
				return result;
			}

			return context.TransitionResource(
				destination.GetNativeResource(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				finalState);
		};

	Result result = recordBuffer(
		gpuData.vertices, uploadData.vertices, 
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	if (!result.IsSuccess())
	{
		return result;
	}

	if (indexed)
	{
		result = recordBuffer(
			gpuData.indices, uploadData.indices,
			D3D12_RESOURCE_STATE_INDEX_BUFFER);

		if (!result.IsSuccess())
		{
			return result;
		}
	}

	return {};
}

}