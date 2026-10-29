#pragma once

#include "DescriptorHeap.h"

namespace rendering::backend
{

class DepthTexture
{
public:
	Result Ensure(ID3D12Device* device, UINT width, UINT height);
	ID3D12Resource* Resource() const noexcept
	{
		return m_resource.Get();
	}
	D3D12_CPU_DESCRIPTOR_HANDLE Dsv() const noexcept
	{
		return m_dsvHandle;
	}
	D3D12_RESOURCE_STATES submittedState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	bool contentsValid = false;

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
	DescriptorHeap m_dsvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle{};
};

}