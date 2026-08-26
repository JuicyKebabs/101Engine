#pragma once
#include <d3d12.h>
#include <cstdint>
#include "Engine/Core/ComPtr/ComPtr.h"

class CommandQueue
{
public:
	CommandQueue() = default;
	~CommandQueue();
	CommandQueue(const CommandQueue&) = delete;
	CommandQueue& operator=(const CommandQueue&) = delete;

	bool Initialize(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type);

	// Execute the given command list and return the fence value 
	// that represents the end of this command list execution.
	uint64_t Execute(ID3D12CommandList* pCommandList);

	// Check if the fence has been completed for the given fence value.
	bool IsFenceComplete(uint64_t fenceValue) const;

	// Wait for the fence to be completed for the given fence value.
	bool WaitForFence(uint64_t fenceValue);

	// Flush the command queue by signaling and waiting for the fence to complete.
	bool Flush();

	ID3D12CommandQueue* GetNativeQueue() const { return m_pCommandQueue.Get(); }

private:
	ComPtr<ID3D12CommandQueue> m_pCommandQueue;
	ComPtr<ID3D12Fence> m_pFence;

	uint64_t m_nextFenceValue = 1;
	HANDLE m_fenceEvent = nullptr;

	D3D12_COMMAND_LIST_TYPE m_type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	uint64_t Signal();	// Signal the command queue and return the fence value for the current state.
};