#include "DescriptorHeap.h"
#include "ApiResult.h"

#include <utility>

namespace rendering::backend
{

using Microsoft::WRL::ComPtr;

Result DescriptorHeap::Initialize(
	ID3D12Device* device,
	D3D12_DESCRIPTOR_HEAP_TYPE type,
	uint32_t descriptorCount,
	bool shaderVisible)
{
	if (IsInitialized())
	{
		return { false, "DescriptorHeap is already initialized." };
	}

	if (device == nullptr)
	{
		return { false, "DescriptorHeap requires a valid device." };
	}

	if (descriptorCount == 0)
	{
		return { false, "DescriptorHeap requires a descriptor count greater than zero." };
	}

	// Check the consistency of the shader visibility flag with the descriptor heap type.
	if (shaderVisible
		&& type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		&& type != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
	{
		return { false, "Only CBV_SRV_UAV and SAMPLER heaps can be shader-visible." };
	}

	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type = type;
	desc.NumDescriptors = descriptorCount;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;

	// Create the descriptor heap in local scope to ensure proper cleanup in case of failure.
	ComPtr<ID3D12DescriptorHeap> heap;

	// Create the descriptor heap
	const HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));

	if (FAILED(hr))
	{
		return MakeApiFailure("CreateDescriptorHeap", hr);
	}

	// Get the descriptor size for the specified type
	const UINT descriptorSize = device->GetDescriptorHandleIncrementSize(type);

	// Move the created objects into the member variables
	m_heap = std::move(heap);
	m_descriptorSize = descriptorSize;
	m_descriptorCount = descriptorCount;
	m_shaderVisible = shaderVisible;

	return {};
}

void DescriptorHeap::Finalize() noexcept
{
	m_heap.Reset();
	m_descriptorSize = 0;
	m_descriptorCount = 0;
	m_shaderVisible = false;
}

Result DescriptorHeap::GetCpuHandle(uint32_t index, D3D12_CPU_DESCRIPTOR_HANDLE& outHandle) const
{
	outHandle = {};

	if (!IsInitialized())
	{
		return { false, "DescriptorHeap is not initialized." };
	}

	if (index >= m_descriptorCount)
	{
		return { false, "DescriptorHeap index is out of range." };
	}

	outHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
	outHandle.ptr += static_cast<SIZE_T>(index) * m_descriptorSize;

	return {};
}

Result DescriptorHeap::GetGpuHandle(std::uint32_t index, D3D12_GPU_DESCRIPTOR_HANDLE& outHandle) const
{
	outHandle = {};

	if (!m_shaderVisible)
	{
		return { false, "DescriptorHeap is not shader-visible." };
	}

	if (!IsInitialized())
	{
		return { false, "DescriptorHeap is not initialized." };
	}

	if (index >= m_descriptorCount)
	{
		return { false, "DescriptorHeap index is out of range." };
	}

	outHandle = m_heap->GetGPUDescriptorHandleForHeapStart();
	outHandle.ptr += static_cast<UINT64>(index) * m_descriptorSize;
	return {};

}

}