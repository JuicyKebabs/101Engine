// R7 implementation guide: section 4.11.
// Window identity survives a partially failed resize, but usable BackBuffers may not.
// IsInitialized answers identity; buffer-ready state answers whether drawing/Present is possible.
// Borrowed BackBuffer information must not outlive Resize or Finalize.

#pragma once

#include "Renderer/Result.h"
#include "Renderer/Win32WindowOutputDesc.h"
#include "DescriptorHeap.h"

#include <array>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

//----------------------------------------------------------------------------------------------------
// SwapChain class
// Represents a DirectX 12 swap chain, which manages the back buffers for rendering and presentation.
//----------------------------------------------------------------------------------------------------

namespace rendering::backend
{

// Native output snapshot, not resource ownership and not Graph storage.
struct BackBufferInfo
{
	ID3D12Resource* resource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
	DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN;
	D3D12_RESOURCE_STATES submittedState = D3D12_RESOURCE_STATE_COMMON;

	bool IsPresentable() const noexcept { return true; }
	ID3D12Resource* Resource() const noexcept { return resource; }
	D3D12_CPU_DESCRIPTOR_HANDLE Rtv() const noexcept { return rtv; }
	DXGI_FORMAT ViewFormat() const noexcept { return viewFormat; }
	D3D12_RESOURCE_STATES SubmittedState() const noexcept { return submittedState; }
};

class SwapChain
{
public:
	static constexpr UINT BufferCount = 2;

	SwapChain() = default;
	~SwapChain();

	SwapChain(const SwapChain&) = delete;
	SwapChain& operator=(const SwapChain&) = delete;
	SwapChain(SwapChain&&) = delete;
	SwapChain& operator=(SwapChain&&) = delete;

	Result Initialize(
		ID3D12Device* device,
		IDXGIFactory4* factory,
		ID3D12CommandQueue* commandQueue,
		const Win32WindowOutputDesc& outputDesc);

	void Finalize() noexcept;

	// Caller has completed submitted work and discarded lists borrowing the buffers.
	Result Resize(ID3D12Device* device, std::uint32_t width, std::uint32_t height);
	Result GetSize(std::uint32_t& width, std::uint32_t& height) const;
	bool IsReady() const noexcept { return m_buffersReady; }

	// Borrowed until Resize or Finalize. Only the current buffer may start new work.
	Result GetCurrentTexture(BackBufferInfo& output) const;

	// Caller has validated identity before submission; owner storage stays stable.
	void CommitSubmittedState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) noexcept;

	// Request presentation.
	// This does not wait for GPU completion.
	Result Present(UINT syncInterval, UINT flags);

	[[nodiscard]]
	bool IsInitialized() const noexcept
	{
		return m_swapChain != nullptr;
	}

private:
	static constexpr DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain;

	struct BackBuffer
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_RESOURCE_STATES submittedState = D3D12_RESOURCE_STATE_PRESENT;
	};

	std::array<BackBuffer, BufferCount> m_backBuffers;

	DescriptorHeap m_rtvHeap;
	bool m_buffersReady = false;

	Result RebuildBackBuffers(ID3D12Device* device);
};

}
