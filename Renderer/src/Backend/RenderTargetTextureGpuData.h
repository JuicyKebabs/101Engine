// R7 implementation guide: section 4.7.
// Physical owner for one render-target Content revision.
// The color image has two views (RTV for writing, SRV for sampling), not two color allocations.
// The matching Depth belongs to this output; Graph and draw code borrow this complete set.

#pragma once

#include "DepthTexture.h"
#include "Renderer/RenderTargetTexture.h"

#include <memory>

namespace rendering::backend
{

// One revision owns the color image, its CPU descriptors, and the scene Depth.
// Neither Graph imports nor Material texture references take this ownership.
class RenderTargetTextureGpuData
{
public:
	Result Initialize(ID3D12Device* device, const RenderTargetTextureDesc& description);

	ID3D12Resource* Resource() const noexcept { return m_color.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE Rtv() const noexcept { return m_rtv; }
	D3D12_CPU_DESCRIPTOR_HANDLE Srv() const noexcept { return m_srv; }
	DepthTexture& Depth() noexcept { return *m_depth; }
	const DepthTexture& Depth() const noexcept { return *m_depth; }

	// Track the last successfully submitted usage, not speculative recording state.
	// contentsValid independently tracks whether Clear/Draw has established readable pixel contents.
	D3D12_RESOURCE_STATES submittedState = D3D12_RESOURCE_STATE_COMMON;
	bool contentsValid = false;
	DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN;
	std::uint32_t width = 0;
	std::uint32_t height = 0;

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_color;
	std::unique_ptr<DescriptorHeap> m_rtvHeap;
	std::unique_ptr<DescriptorHeap> m_srvHeap;
	std::unique_ptr<DepthTexture> m_depth;
	D3D12_CPU_DESCRIPTOR_HANDLE m_rtv{};
	D3D12_CPU_DESCRIPTOR_HANDLE m_srv{};
};

}
