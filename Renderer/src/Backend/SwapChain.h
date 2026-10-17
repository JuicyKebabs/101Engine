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

	// Returns borrowed references to the current back buffer and its RTV.
	Result GetCurrentBackBuffer(
		ID3D12Resource*& outResource,
		D3D12_CPU_DESCRIPTOR_HANDLE& outRtvHandle) const;

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

	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, BufferCount> m_backBuffers;

	DescriptorHeap m_rtvHeap;
};

}