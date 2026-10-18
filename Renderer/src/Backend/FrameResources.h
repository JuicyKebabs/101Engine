#pragma once

#include "Device.h"
#include"Buffer.h"
#include"DescriptorHeap.h"

#include<cstdint>
#include<span>
#include<vector>

namespace rendering::backend
{

struct ConstantAllocation
{
	D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
	UINT sizeInBytes = 0;
};

class FrameResources
{
public:
	Result Initialize(
		Device* device,
		UINT constantBytes,
		UINT descriptorCount,
		UINT retainedCount);

	void Finalize() noexcept;

	void ResetAfterCompletion() noexcept;

	Result AllocateConstants(std::span<const std::byte> data, ConstantAllocation& outAPut);

	Result CopyDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE source, D3D12_GPU_DESCRIPTOR_HANDLE& output);

	Result KeepAlive(IUnknown* object);

	[[nodiscard]]
	ID3D12DescriptorHeap* GetDescriptorHeap() const noexcept
	{
		return m_descriptors.GetNativeHeap();
	}

private:
	Device* m_device = nullptr;
	Buffer m_constants;
	DescriptorHeap m_descriptors;
	std::vector<Microsoft::WRL::ComPtr<IUnknown>> m_retainedObjects;
	std::uint64_t m_constantOffset = 0;
	UINT m_descriptorOffset = 0;
	UINT m_descriptorCapacity = 0;
	UINT m_retainedCapacity = 0;

};




}