#pragma once

#include "Renderer/Result.h"

#include <Windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

//--------------------------------------------------------------------------
// CommandQueue class
// Represents a DirectX 12 command queue and fence for synchronization.
//--------------------------------------------------------------------------

namespace rendering::backend
{

class CommandContext;

// struct to hold the result of a queue submission operation.
struct [[nodiscard]] QueueSubmissionResult
{
	Result status;

	// If the ExecuteCommandLists was called.
	// This flag does not indicate the success and completion of the GPU execution.
	bool executionIssued = false;

	// Stores valid value. Zero for failure.
	uint64_t fenceValue = 0;
};

class CommandQueue
{
public:
	CommandQueue() = default;
	~CommandQueue();
	
	CommandQueue(const CommandQueue&) = delete;
	CommandQueue& operator=(const CommandQueue&) = delete;
	CommandQueue(CommandQueue&&) = delete;
	CommandQueue& operator=(CommandQueue&&) = delete;

	Result Initialize(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);
	void Finalize() noexcept;

	// Returns the operation result.
	// On success, outFenceValue receives the scheduled fence value.
	// On failure, outFenceValue is 0.
	Result Signal(uint64_t& outFenceValue);

	// Waits for the fence to reach the specified value.
	Result WaitForFence(uint64_t fenceValue);

	// Waits for the completion of all previously submitted commands in the queue.
	Result Flush();

	// Execute the command list from the given CommandContext.
	// Call after the CommandContext is closed and the GPU execution is finished.
	QueueSubmissionResult Execute(const CommandContext& context);

	// Queries completion without waiting.
	// outCompleted is meaningful only when the result is success.
	[[nodiscard]]
	Result QueryFenceCompletion(std::uint64_t fenceValue, bool& outCompleted);

	[[nodiscard]]
	Result QueryCompletedFenceValue(std::uint64_t& outValue);

	[[nodiscard]]
	bool IsInitialized() const 
	{
		return m_commandQueue != nullptr
			&& m_fence != nullptr
			&& m_fenceEvent != nullptr;
	}

	[[nodiscard]]
	ID3D12CommandQueue* GetNativeCommandQueue() const noexcept { return m_commandQueue.Get(); }

private:
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	HANDLE m_fenceEvent = nullptr;

	uint64_t m_nextFenceValue = 1;
	bool m_syncFailed = false;
};

}