#pragma once

#include "DescriptorHeap.h"

namespace rendering::backend
{

class DepthTexture
{
public:
	Result Ensure(ID3D12Device* device, UINT width, UINT height, bool allowSampling = false);
	ID3D12Resource* Resource() const noexcept
	{
		return m_resource.Get();
	}
	D3D12_CPU_DESCRIPTOR_HANDLE Dsv() const noexcept
	{
		return m_dsvHandle;
	}
    // A sampling descriptor exists only for explicitly shader-readable depth.
    D3D12_CPU_DESCRIPTOR_HANDLE Srv() const noexcept { return m_srvHandle; }
    bool AllowsSampling() const noexcept { return m_srvHandle.ptr != 0; }

	D3D12_RESOURCE_STATES submittedState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	bool contentsValid = false;

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
	DescriptorHeap m_dsvHeap;
    DescriptorHeap m_srvHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle{};
	D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle{};
};

}