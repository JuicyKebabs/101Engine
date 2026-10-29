// R7 implementation guide: section 4.15.
// One Render-long connection between a Graph identity and an external resource owner.
// The owner variant identifies where to re-query and commit state; it does not own the pointer.
// Writing and sampling the same output must resolve to the same imported Graph identity.

#pragma once

#include "GraphResource.h"

#include <d3d12.h>
#include <variant>

namespace rendering::detail { struct TextureContent; }
namespace rendering::detail { struct RenderTargetTextureContent; }
namespace rendering::backend { class DepthTexture; }

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

	D3D12_CPU_DESCRIPTOR_HANDLE srv{};
	D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
	bool contentsValid = false;

	// Non-owning identity for QueryBinding/CommitBinding; owners outlive execution.
	// monostate identifies the current Window BackBuffer managed by Backend.
	std::variant<std::monostate, detail::TextureContent*, backend::DepthTexture*, detail::RenderTargetTextureContent*> owner;

	ID3D12Resource* Resource() const noexcept { return resource; }
	D3D12_CPU_DESCRIPTOR_HANDLE Rtv() const noexcept { return rtv; }
	DXGI_FORMAT ViewFormat() const noexcept { return viewFormat; }
	D3D12_RESOURCE_STATES SubmittedState() const noexcept { return initialState; }
	bool IsPresentable() const noexcept { return presentable; }
};

}
