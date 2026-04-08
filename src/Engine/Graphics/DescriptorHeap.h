#pragma once
#include <d3d12.h>
#include <d3dx12.h>
#include <assert.h>
#include "Engine/Core/ComPtr/ComPtr.h"

// Descriptor heap class
class DescriptorHeap
{
public:
	enum class Type
	{
		RTV,			// Render Target View
		DSV,			// Depth Stencil View
		CBV_SRV_UAV,	// Constant Buffer View / Shader Resource View / Unordered Access View
		SAMPLER,		// Sampler
	};

	static D3D12_DESCRIPTOR_HEAP_TYPE ConvertToD3D12Type(Type type)
	{
		switch (type)
		{
		case Type::RTV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		case Type::DSV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		case Type::CBV_SRV_UAV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		case Type::SAMPLER:
			return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		default:
			assert(false && "Invalid descriptor heap type");
			return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		}
	}

public:
	DescriptorHeap() = default;
	~DescriptorHeap() = default;

	void Initialize(ID3D12Device* pDevice, DescriptorHeap::Type type, UINT numDescriptors);

	uint32_t AllocateDescriptor() { return m_nextFreeIndex < m_numDescriptors ? m_nextFreeIndex++ : UINT32_MAX; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32_t index) const{
		assert(index < m_numDescriptors && "Descriptor index out of bounds");
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heap->GetCPUDescriptorHandleForHeapStart(), index, m_descriptorSize);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32_t index) const{
		if(m_type == Type::RTV || m_type == Type::DSV){
			assert(false && "RTV and DSV heaps are not shader visible, cannot get GPU handle");
			return CD3DX12_GPU_DESCRIPTOR_HANDLE();
		}
		assert(index < m_numDescriptors && "Descriptor index out of bounds");
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heap->GetGPUDescriptorHandleForHeapStart(), index, m_descriptorSize);
	}

	ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }
	UINT GetDescriptorSize() const { return m_descriptorSize; }
	UINT GetNumDescriptors() const { return m_numDescriptors; }

private:
	ComPtr<ID3D12DescriptorHeap> m_heap;
	UINT m_descriptorSize = 0;
	UINT m_numDescriptors = 0;
	uint32_t m_nextFreeIndex = 0;
	DescriptorHeap::Type m_type = DescriptorHeap::Type::CBV_SRV_UAV;
};