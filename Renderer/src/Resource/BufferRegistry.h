#pragma once

#include "Backend/StructuredBufferGpuData.h"
#include "Renderer/StructuredBufferDesc.h"
#include "VersionedRegistry.h"

namespace rendering::detail
{

class BufferRegistry
{
public:
	explicit BufferRegistry(std::uint32_t ownerId) : m_buffers(ownerId)
	{}

	ResourceRegistrationResult<BufferHandle> CreateStructuredBuffer(
		backend::Backend& backend,
		const StructuredBufferDesc& desc);

	Result UpdateStructuredBuffer(
		backend::Backend& backend,
		BufferHandle handle,
		std::span<const std::byte> data);

	Result Resolve(
		BufferHandle handle,
		std::uint32_t expectedStride,
		const backend::StructuredBufferGpuData*& gpuData);

	Result Destroy(BufferHandle handle) { return m_buffers.Destroy(handle); }

private:
	struct BufferContent
	{
		backend::StructuredBufferGpuData gpuData;
		bool contentsInitialized = false;
	};

	VersionedRegistry<BufferHandle, BufferContent> m_buffers;
};

}