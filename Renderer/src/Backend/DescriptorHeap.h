#pragma once

#include "Renderer/Result.h"

#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>

namespace rendering::backend
{

class DescriptorHeap
{
public:
	DescriptorHeap() = default;
	~DescriptorHeap() = default;

	DescriptorHeap(const DescriptorHeap&) = delete;
	DescriptorHeap& operator=(const DescriptorHeap&) = delete;
	DescriptorHeap(DescriptorHeap&&) = delete;
	DescriptorHeap& operator=(DescriptorHeap&&) = delete;

	Result Initialize(
		ID3D12Device* device,
		D3D12_DESCRIPTOR_HEAP_TYPE type,
		uint32_t descriptorCount,
		bool shaderVisible = false);

	void Finalize() noexcept;

	Result GetCpuHandle(uint32_t index, D3D12_CPU_DESCRIPTOR_HANDLE& outHandle) const;

	[[nodiscard]]
	bool IsInitialized() const { return m_heap != nullptr; }

private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;

	UINT m_descriptorSize = 0;
	uint32_t m_descriptorCount = 0;
};

}