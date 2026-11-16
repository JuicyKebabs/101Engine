#include "BufferRegistry.h"
#include "Failure.h"

#include <limits>
#include <utility>

namespace rendering::detail
{

ResourceRegistrationResult<BufferHandle> BufferRegistry::CreateStructuredBuffer(
	backend::Backend& backend,
	const StructuredBufferDesc& desc)
{
	auto candidate = std::make_unique<BufferContent>();
	ResourceRegistrationResult<BufferHandle> registration;

	// Initialize gpuData for the structured buffer (resource and SRV descriptor heap).
	registration.result = candidate->gpuData.Initialize(backend, desc);

	if (!registration.result.IsSuccess())
	{
		return registration;
	}

	// Register the buffer in the registry and return the handle.
	return m_buffers.Register(std::move(candidate));
}

Result BufferRegistry::UpdateStructuredBuffer(
	backend::Backend& backend,
	BufferHandle handle,
	std::span<const std::byte> data)
{
	auto* content = m_buffers.FindForNewUse(handle);

	if (!content)
	{
		return Fail("UpdateBuffer: " + m_buffers.DescribeUnavailable(handle));
	}

	// Data size is not flexible currently,
	// so it must match the buffer size exactly.
	if (data.size() != content->gpuData.GetSize())
	{
		return Fail("UpdateBuffer: Data size does not match buffer size.");
	}

	// Write the data to the GPU buffer.
	Result result = content->gpuData.Write(backend, data);

	if (!result.IsSuccess())
	{
		return result;
	}

	// Mark the buffer contents as initialized.
	content->contentsInitialized = true;
	return {};
}

Result BufferRegistry::Resolve(
	BufferHandle handle,
	std::uint32_t expectedStride,
	const backend::StructuredBufferGpuData*& outGpuData)
{
	const auto* content = m_buffers.FindForNewUse(handle);

	if (!content)
	{
		return Fail("StructuredBuffer unavailable: " + m_buffers.DescribeUnavailable(handle));
	}

	if (!content->contentsInitialized)
	{
		return Fail("StructuredBuffer is uninitialized.");
	}

	if (content->gpuData.GetElementStride() != expectedStride)
	{
		return Fail("StructuredBuffer stride mismatch.");
	}

	outGpuData = &content->gpuData;
	return {};
}

}