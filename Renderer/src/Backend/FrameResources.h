// R7 implementation guide: section 4.25.
// Owns a linear constant Upload Buffer, shader-visible descriptors and COM keep-alive references.
// Growth happens before any addresses/handles are handed to this frame's commands.
// Reset is completion-gated; retained capacity is a high-water mark rather than a ring buffer.

#pragma once

#include "Device.h"
#include"Buffer.h"
#include"DescriptorHeap.h"

#include<memory>
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

	// Only before any allocations, after the previous use has completed.
	Result EnsureCapacity(UINT constantBytes, UINT descriptorCount, UINT retainedCount);

	void Finalize() noexcept;

	void ResetAfterCompletion() noexcept;

	Result AllocateConstants(std::span<const std::byte> data, ConstantAllocation& outAPut);

	Result CopyDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE source, D3D12_GPU_DESCRIPTOR_HANDLE& output);

	Result KeepAlive(IUnknown* object);

	[[nodiscard]]
	ID3D12DescriptorHeap* GetDescriptorHeap() const noexcept
	{
		return m_descriptors ? m_descriptors->GetNativeHeap() : nullptr;
	}

private:
	Device* m_device = nullptr;
	std::unique_ptr<Buffer> m_constants;
	std::unique_ptr<DescriptorHeap> m_descriptors;
	std::vector<Microsoft::WRL::ComPtr<IUnknown>> m_retainedObjects;
	std::uint64_t m_constantOffset = 0;
	UINT m_descriptorOffset = 0;
	UINT m_descriptorCapacity = 0;
	UINT m_retainedCapacity = 0;

};




}