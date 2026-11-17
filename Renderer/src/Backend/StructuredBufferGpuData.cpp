#include "StructuredBufferGpuData.h"
#include "ApiResult.h"
#include "Backend.h"
#include "Failure.h"

#include <limits>
#include <utility>

namespace rendering::backend
{

namespace
{

Result ClassifyBufferFailure(ID3D12Device* device, Result result)
{
	// Detecting device lost.
	if (!result.IsSuccess() && device && FAILED(device->GetDeviceRemovedReason()))
	{
		result.failureKind = FailureKind::DeviceLost;
	}

	return result;
}

}

Result StructuredBufferGpuData::Initialize(Backend& backend, const StructuredBufferDesc& desc)
{
	ID3D12Device* device = backend.GetNativeDevice();

	// Limit the number of elements to the maximum allowed by D3D12.
	constexpr std::uint32_t kMaxElementCount = 1u << D3D12_REQ_BUFFER_RESOURCE_TEXEL_COUNT_2_TO_EXP;

	if (desc.elementCount == 0 || desc.elementCount > kMaxElementCount)
	{
		return detail::Fail("CreateStructuredBuffer: element count is out of range.");
	}

	// Limit the stride to a multiple of 4 and within the maximum allowed by D3D12.
	if (desc.elementStride == 0 ||
		desc.elementStride % 4 != 0 ||
		desc.elementStride > D3D12_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES)
	{
		return detail::Fail("CreateStructuredBuffer: stride must be a multiple of 4 in [4, 2048].");
	}

	// Calculate the total byte size of the buffer and ensure it does not exceed the CPU addressable range.
	const auto byteSize = static_cast<std::uint64_t>(desc.elementCount) * desc.elementStride;

	if (byteSize > (std::numeric_limits<std::size_t>::max)())
	{
		return detail::Fail("CreateStructuredBuffer: byte size exceeds the CPU addressable range.");
	}

	// Initialisze buffer as upload buffer to allow CPU writes.
	Result result = m_buffer.Initialize(device, byteSize, BufferMemory::Upload);

	if (!result.IsSuccess())
	{
		return ClassifyBufferFailure(device, std::move(result));
	}

	// Initialize the heap as a shader resource view (SRV) descriptor heap with a single descriptor.
	result = m_srvHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);

	if (!result.IsSuccess())
	{
		return ClassifyBufferFailure(device, std::move(result));
	}

	// Get the CPU descriptor handle for the SRV from the descriptor heap.
	result = m_srvHeap.GetCpuHandle(0, m_srvHandle);

	if (!result.IsSuccess())
	{
		return ClassifyBufferFailure(device, std::move(result));
	}

	// Create the shader resource view (SRV) for the structured buffer.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN; // Structured buffers do not have a specific format.
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = desc.elementCount;
	srvDesc.Buffer.StructureByteStride = desc.elementStride;

	device->CreateShaderResourceView(m_buffer.GetNativeResource(), &srvDesc, m_srvHandle);

	// Check if the device has been removed after creating the SRV.
	const HRESULT deviceStatus = device->GetDeviceRemovedReason();

	if (FAILED(deviceStatus))
	{
		Result failure = MakeApiFailure("CreateShaderResourceView(StructuredBuffer): device removed", deviceStatus);
		failure.failureKind = FailureKind::DeviceLost;
		return failure;
	}

	m_elementStride = desc.elementStride;

	return {};
}

Result StructuredBufferGpuData::Write(Backend& backend, std::span<const std::byte> data)
{
	Result written = m_buffer.Write(data);
	return ClassifyBufferFailure(backend.GetNativeDevice(), std::move(written));
}

Result StructuredBufferGpuData::CopyFrameSrv(Backend& backend, D3D12_GPU_DESCRIPTOR_HANDLE& output) const
{
	return backend.CopyFrameDescriptor(m_srvHandle, output);
}

}