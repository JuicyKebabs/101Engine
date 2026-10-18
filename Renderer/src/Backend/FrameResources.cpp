#include "FrameResources.h"

#include<array>
#include<cstddef>
#include<new>
#include<stdexcept>

namespace rendering::backend
{

Result FrameResources::Initialize(
	Device* device,
	UINT constantBytes,
	UINT descriptorCount,
	UINT retainedCount)
{
	if (!device->IsInitialized())
	{
		return { false, "Device is not initialized." };
	}

	if (constantBytes == 0 || descriptorCount == 0 || retainedCount == 0)
	{
		return { false, "Invalid parameters for FrameResources initialization." };
	}

	try
	{
		m_retainedObjects.reserve(retainedCount);
	}
	catch (const std::bad_alloc&)
	{
		return { false, "Frame retained storage allocation failed." };
	}
	catch (const std::length_error&)
	{
		return { false, "Frame retained storage capacity is unsupported." };
	}

	Result result = m_constants.Initialize(device->GetNativeDevice(), constantBytes, BufferMemory::Upload);

	if (!result.IsSuccess())
	{
		return result;
	}

	result = m_descriptors.Initialize(device->GetNativeDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptorCount, true);

	if (!result.IsSuccess())
	{
		m_constants.Finalize();
		return result;
	}

	m_device = device;
	m_descriptorCapacity = descriptorCount;
	m_retainedCapacity = retainedCount;

	return { true, "" };
}

void FrameResources::Finalize() noexcept
{
	ResetAfterCompletion();
	m_descriptors.Finalize();
	m_constants.Finalize();
	m_device = nullptr;
	m_descriptorCapacity = 0;
	m_retainedCapacity = 0;
}

void FrameResources::ResetAfterCompletion() noexcept
{
	m_constantOffset = 0;
	m_descriptorOffset = 0;
	m_retainedObjects.clear();
}

Result FrameResources::AllocateConstants(std::span<const std::byte> data, ConstantAllocation& output)
{
	constexpr UINT alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	constexpr UINT maxBytes = D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;

	if (!m_device->IsInitialized())
	{
		return { false, "Device is not initialized." };
	}

	if (data.size() == 0 || data.size() > maxBytes)
	{
		return { false, "Invalid constant data size." };
	}

	const UINT padded = (static_cast<UINT>((data.size())) + alignment - 1) &~(alignment - 1);

	if (m_constantOffset > m_constants.GetSize()
		|| padded > m_constants.GetSize() - m_constantOffset)
	{
		return { false, "Frame constant capacity exceeded." };
	}

	Result result = m_constants.WriteAt(m_constantOffset, data);

	if (!result.IsSuccess())
	{
		return result;
	}

	const UINT padding = padded - static_cast<UINT>(data.size());
	if (padding > 0)
	{
		const std::array<std::byte, alignment> zeros{};
		result = m_constants.WriteAt(
			m_constantOffset + data.size(),
			std::span<const std::byte>(zeros.data(), padding));

		if (!result.IsSuccess())
		{
			return result;
		}
	}

	output = { m_constants.GetNativeResource()->GetGPUVirtualAddress() + m_constantOffset, padded };

	m_constantOffset += padded;

	return {};
}

Result FrameResources::CopyDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE source, D3D12_GPU_DESCRIPTOR_HANDLE& output)
{
	if (!m_device->IsInitialized())
	{
		return { false, "Device is not initialized." };
	}

	if (!source.ptr)
	{
		return { false, "Source descriptor handle is null." };
	}

	if (m_descriptorOffset >= m_descriptorCapacity)
	{
		return { false, "Frame descriptor capacity exceeded." };
	}

	D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
	D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
	Result result = m_descriptors.GetCpuHandle(m_descriptorOffset, cpu);

	if (!result.IsSuccess())
	{
		return result;
	}

	result = m_descriptors.GetGpuHandle(m_descriptorOffset, gpu);

	if (!result.IsSuccess())
	{
		return result;
	}

	auto nativeDevice = m_device->GetNativeDevice();

	nativeDevice->CopyDescriptorsSimple(
		1,
		cpu,
		source,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	output = gpu;
	++m_descriptorOffset;

	return {};
}

Result FrameResources::KeepAlive(IUnknown* object)
{
	if (!m_device->IsInitialized())
	{
		return { false, "Device is not initialized." };
	}

	if (!object)
	{
		return { false, "Cannot keep a null object alive." };
	}

	if (m_retainedObjects.size() >= m_retainedCapacity)
	{
		return { false, "Frame retained capacity exceeded." };
	}

	m_retainedObjects.emplace_back(object);

	return {};
}

}