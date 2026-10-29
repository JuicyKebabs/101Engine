// R7 implementation guide: section 4.26.
// Grow complete replacement allocations before recording, then allocate monotonically.
// Constants, descriptor slots and retained references have separate capacities and hard limits.
// GPU completion is required before resetting offsets or releasing references.

#include "FrameResources.h"
#include "Failure.h"

#include <algorithm>
#include <array>
#include <new>
#include <stdexcept>

namespace rendering::backend
{

namespace
{

constexpr UINT MaximumConstantBytes = 64 * 1024 * 1024;
constexpr UINT MaximumDescriptorCount = 65536;
constexpr UINT MaximumRetainedCount = 262144;

Result ClassifyDeviceFailure(Device& device, Result failure)
{
	Result deviceStatus = device.CheckStatus();
	if (!deviceStatus.IsSuccess())
	{
		deviceStatus.message = failure.message + "\n" + deviceStatus.message;
		return deviceStatus;
	}

	return failure;
}

// Grow geometrically up to a validated hard limit and never shrink on a smaller later frame.
// Retained high-water capacity is different from live allocations or leaked references.
UINT GrowthCapacity(UINT current, UINT required, UINT maximum)
{
	UINT capacity = (std::max)(current, UINT{ 1 });
	while (capacity < required)
	{
		capacity = capacity > maximum / 2 ? maximum : capacity * 2;
	}

	return capacity;
}

}

Result FrameResources::Initialize(
	Device* device,
	UINT constantBytes,
	UINT descriptorCount,
	UINT retainedCount)
{
	if (!device || !device->IsInitialized())
	{
		return detail::Fail("FrameResources: initialized Device required.");
	}

	if (m_device)
	{
		return detail::Fail("FrameResources: already initialized.");
	}

	if (constantBytes == 0 || descriptorCount == 0 || retainedCount == 0)
	{
		return detail::Fail("FrameResources: initial capacities must be nonzero.");
	}

	m_device = device;
	Result prepared = EnsureCapacity(constantBytes, descriptorCount, retainedCount);
	if (!prepared.IsSuccess())
	{
		Finalize();
	}

	return prepared;
}

// Precondition: previous GPU use is complete and this frame has allocated nothing yet.
// Create replacement Buffer/Heap privately, reserve keep-alive storage, then commit all ownership.
// If preparation fails, old allocations remain intact and no previously handed-out addresses change.
Result FrameResources::EnsureCapacity(UINT constantBytes, UINT descriptorCount, UINT retainedCount)
{
	if (!m_device || !m_device->IsInitialized())
	{
		return detail::Fail("FrameResources: Device is unavailable.");
	}

	const bool hasAllocations = m_constantOffset != 0 || m_descriptorOffset != 0 || !m_retainedObjects.empty();
	if (hasAllocations)
	{
		return detail::Fail("FrameResources: capacity cannot change after allocation begins.");
	}

	if (constantBytes > MaximumConstantBytes || descriptorCount > MaximumDescriptorCount)
	{
		return detail::Fail("FrameResources: constant or Descriptor hard limit exceeded.");
	}

	if (retainedCount > MaximumRetainedCount)
	{
		return detail::Fail("FrameResources: retained-reference hard limit exceeded.");
	}

	const UINT currentConstantBytes = m_constants ? static_cast<UINT>(m_constants->GetSize()) : 0;
	const UINT desiredConstantBytes = GrowthCapacity(currentConstantBytes, constantBytes, MaximumConstantBytes);
	const UINT desiredDescriptors = GrowthCapacity(m_descriptorCapacity, descriptorCount, MaximumDescriptorCount);
	const UINT desiredRetained = GrowthCapacity(m_retainedCapacity, retainedCount, MaximumRetainedCount);

	try
	{
		std::unique_ptr<Buffer> replacementConstants;
		if (!m_constants || desiredConstantBytes > currentConstantBytes)
		{
			replacementConstants = std::make_unique<Buffer>();
			Result created = replacementConstants->Initialize(m_device->GetNativeDevice(), desiredConstantBytes, BufferMemory::Upload);
			if (!created.IsSuccess())
			{
				return ClassifyDeviceFailure(*m_device, std::move(created));
			}
		}

		std::unique_ptr<DescriptorHeap> replacementDescriptors;
		if (!m_descriptors || desiredDescriptors > m_descriptorCapacity)
		{
			replacementDescriptors = std::make_unique<DescriptorHeap>();
			Result created = replacementDescriptors->Initialize(
				m_device->GetNativeDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, desiredDescriptors, true);
			if (!created.IsSuccess())
			{
				return ClassifyDeviceFailure(*m_device, std::move(created));
			}
		}

		// Finish the last potentially throwing CPU capacity operation before exchanging either GPU owner.
		// Doing this after exchanging the Buffer would leave a partial capacity update on allocation failure.
		m_retainedObjects.reserve(desiredRetained);

		// No GPU references have been handed out, so all replacements can commit together.
		if (replacementConstants)
		{
			m_constants = std::move(replacementConstants);
		}

		if (replacementDescriptors)
		{
			m_descriptors = std::move(replacementDescriptors);
		}

		m_descriptorCapacity = desiredDescriptors;
		m_retainedCapacity = desiredRetained;
		return {};
	}
	catch (const std::bad_alloc&)
	{
		return detail::Fail("FrameResources: capacity allocation failed.");
	}
	catch (const std::length_error&)
	{
		return detail::Fail("FrameResources: capacity is unsupported.");
	}
}

void FrameResources::Finalize() noexcept
{
	ResetAfterCompletion();
	m_descriptors.reset();
	m_constants.reset();
	m_device = nullptr;
	m_descriptorCapacity = 0;
	m_retainedCapacity = 0;
}

// The caller, not this small helper, proves Fence completion.
// Resetting offsets while commands are pending would let the CPU overwrite Camera data or SRVs still in use.
void FrameResources::ResetAfterCompletion() noexcept
{
	m_constantOffset = 0;
	m_descriptorOffset = 0;
	m_retainedObjects.clear();
}

// Reserve a fresh 256-byte-aligned range and zero its padding.
// Return the GPU address only after writes succeed; never wrap around onto another View's constants.
// A single binding is also constrained by DX12's constant-buffer size limit.
Result FrameResources::AllocateConstants(std::span<const std::byte> data, ConstantAllocation& output)
{
	constexpr UINT alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	constexpr UINT maximumBufferBytes = D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;

	if (!m_device || !m_constants)
	{
		return detail::Fail("FrameResources: constants are unavailable.");
	}

	if (data.empty() || data.size() > maximumBufferBytes)
	{
		return detail::Fail("FrameResources: invalid constant block size.");
	}

	const UINT paddedBytes = (static_cast<UINT>(data.size()) + alignment - 1) & ~(alignment - 1);
	if (m_constantOffset > m_constants->GetSize() || paddedBytes > m_constants->GetSize() - m_constantOffset)
	{
		return detail::Fail("Frame constant capacity exceeded.");
	}

	Result written = m_constants->WriteAt(m_constantOffset, data);
	if (!written.IsSuccess())
	{
		return ClassifyDeviceFailure(*m_device, std::move(written));
	}

	const UINT paddingBytes = paddedBytes - static_cast<UINT>(data.size());
	if (paddingBytes != 0)
	{
		const std::array<std::byte, alignment> zeros{};
		written = m_constants->WriteAt(m_constantOffset + data.size(), std::span<const std::byte>(zeros.data(), paddingBytes));
		if (!written.IsSuccess())
		{
			return ClassifyDeviceFailure(*m_device, std::move(written));
		}
	}

	ConstantAllocation allocation;
	allocation.gpuAddress = m_constants->GetNativeResource()->GetGPUVirtualAddress() + m_constantOffset;
	allocation.sizeInBytes = paddedBytes;
	output = allocation;
	m_constantOffset += paddedBytes;
	return {};
}

// Copy one CPU SRV description into the shader-visible frame heap.
// Advance monotonically so a later Draw cannot overwrite a descriptor used by an earlier Draw.
Result FrameResources::CopyDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE source, D3D12_GPU_DESCRIPTOR_HANDLE& output)
{
	if (!m_device || !m_descriptors)
	{
		return detail::Fail("FrameResources: Descriptor storage is unavailable.");
	}

	if (!source.ptr)
	{
		return detail::Fail("FrameResources: source Descriptor is null.");
	}

	if (m_descriptorOffset >= m_descriptorCapacity)
	{
		return detail::Fail("Frame descriptor capacity exceeded.");
	}

	D3D12_CPU_DESCRIPTOR_HANDLE destinationCpu{};
	D3D12_GPU_DESCRIPTOR_HANDLE destinationGpu{};
	Result found = m_descriptors->GetCpuHandle(m_descriptorOffset, destinationCpu);
	if (!found.IsSuccess())
	{
		return found;
	}

	found = m_descriptors->GetGpuHandle(m_descriptorOffset, destinationGpu);
	if (!found.IsSuccess())
	{
		return found;
	}

	m_device->GetNativeDevice()->CopyDescriptorsSimple(
		1, destinationCpu, source, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	output = destinationGpu;
	++m_descriptorOffset;
	return {};
}

// A COM reference keeps the native object alive independently of registry Content replacement.
// The reserved list cannot grow beyond the preflight capacity; release only at completion-gated Reset.
Result FrameResources::KeepAlive(IUnknown* object)
{
	if (!m_device || !m_device->IsInitialized())
	{
		return detail::Fail("FrameResources: Device is unavailable.");
	}

	if (!object)
	{
		return detail::Fail("FrameResources: cannot retain a null object.");
	}

	if (m_retainedObjects.size() >= m_retainedCapacity)
	{
		return detail::Fail("Frame retained capacity exceeded.");
	}

	m_retainedObjects.emplace_back(object);
	return {};
}

}
