#pragma once
#include <d3d12.h>
#include <d3dx12.h>
#include "DescriptorHeap.h"

class DescriptorHeapAllocator
{
public:
	static constexpr uint32_t MAX_CBV_SRV_UAV_DESCRIPTORS = 1000;
	static constexpr uint32_t MAX_RTV_DESCRIPTORS = 100;
	static constexpr uint32_t MAX_DSV_DESCRIPTORS = 100;

public:
	DescriptorHeapAllocator(ID3D12Device* device) : m_pDevice(device) {}
	~DescriptorHeapAllocator() = default;

	DescriptorHeapAllocator(const DescriptorHeapAllocator&) = delete;
	DescriptorHeapAllocator& operator=(const DescriptorHeapAllocator&) = delete;

	void Initialize() {
		assert(m_pDevice && "DescriptorHeapAllocator: Device pointer is null.");
		assert(!m_isInitialized && "DescriptorHeapAllocator: Already initialized.");
		m_cbvSrvUavHeap.Initialize(m_pDevice, DescriptorHeap::Type::CBV_SRV_UAV, MAX_CBV_SRV_UAV_DESCRIPTORS);
		m_rtvHeap.Initialize(m_pDevice, DescriptorHeap::Type::RTV, MAX_RTV_DESCRIPTORS);
		m_dsvHeap.Initialize(m_pDevice, DescriptorHeap::Type::DSV, MAX_DSV_DESCRIPTORS);
		m_isInitialized = true;
	}

	uint32_t AllocateCbvSrvUav() { assert(m_isInitialized && "DescriptorHeapAllocator: Not initialized."); return m_cbvSrvUavHeap.AllocateDescriptor(); }
	uint32_t AllocateRtv() { assert(m_isInitialized && "DescriptorHeapAllocator: Not initialized."); return m_rtvHeap.AllocateDescriptor(); }
	uint32_t AllocateDsv() { assert(m_isInitialized && "DescriptorHeapAllocator: Not initialized."); return m_dsvHeap.AllocateDescriptor(); }

	DescriptorHeap& GetCbvSrvUavHeap() { assert(m_isInitialized && "DescriptorHeapAllocator: Not initialized."); return m_cbvSrvUavHeap; }
	DescriptorHeap& GetRtvHeap() { assert(m_isInitialized && "DescriptorHeapAllocator: Not initialized."); return m_rtvHeap; }
	DescriptorHeap& GetDsvHeap() { assert(m_isInitialized && "DescriptorHeapAllocator: Not initialized."); return m_dsvHeap; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCbvSrvUavCpuHandle(uint32_t index) const { assert(m_isInitialized && "DescriptorHeapAllocator: Not initialized."); return m_cbvSrvUavHeap.GetCpuHandle(index); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRtvCpuHandle(uint32_t index) const { assert(m_isInitialized && "DescriptorHeapAllocator: Not initialized."); return m_rtvHeap.GetCpuHandle(index); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDsvCpuHandle(uint32_t index) const { assert(m_isInitialized && "DescriptorHeapAllocator: Not initialized."); return m_dsvHeap.GetCpuHandle(index); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavGpuHandle(uint32_t index) const { assert(m_isInitialized && "DescriptorHeapAllocator: Not initialized."); return m_cbvSrvUavHeap.GetGpuHandle(index); }

private:
	ID3D12Device* m_pDevice = nullptr;
	DescriptorHeap m_cbvSrvUavHeap;
	DescriptorHeap m_rtvHeap;
	DescriptorHeap m_dsvHeap;

	bool m_isInitialized = false;
};