#pragma once

#include "GraphResource.h"
#include <d3d12.h>

//---------------------------------------------------------------------------------------
// GraphTextureBinding
// It's necessary to store the one on one relationship between resource and it's handle.
// A snapshot is just borrowing target resource for render graph execution.
//---------------------------------------------------------------------------------------

namespace rendering::graph
{

struct GraphTextureBinding
{
	ID3D12Resource* resource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
	DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN;
	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
	bool presentable = false;
	GraphTextureHandle handle;

	ID3D12Resource* Resource() const noexcept { return resource; }
	D3D12_CPU_DESCRIPTOR_HANDLE Rtv() const noexcept { return rtv; }
	DXGI_FORMAT ViewFormat() const noexcept { return viewFormat; }
	D3D12_RESOURCE_STATES SubmittedState() const noexcept { return initialState; }
	bool IsPresentable() const noexcept { return presentable; }
};

}