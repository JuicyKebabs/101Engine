#include "CommandQueue.h"
#include "CommandContext.h"
#include "ApiResult.h"

#include <limits>
#include <utility>

namespace rendering::backend
{

using Microsoft::WRL::ComPtr;

static constexpr auto invalidFenceValue = (std::numeric_limits<uint64_t>::max)();

CommandQueue::~CommandQueue()
{
	Finalize();
}

Result CommandQueue::Initialize(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type)
{
	if (IsInitialized())
	{
		return { false, "CommandQueue is already initialized." };
	}

	if (device == nullptr)
	{
		return { false, "CommandQueue requires a valid device." };
	}

	// Create the command queue and fence in local scope to ensure proper cleanup in case of failure.
	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<ID3D12Fence> fence;

	D3D12_COMMAND_QUEUE_DESC desc{};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	// Create the command queue
	HRESULT hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue));

	if (FAILED(hr))
	{
		return MakeApiFailure("CreateCommandQueue", hr);
	}

	// Create the fence for synchronization
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

	if (FAILED(hr))
	{
		return MakeApiFailure("CreateFence", hr);
	}

	// Create an event for fence synchronization
	const HANDLE fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

	if (fenceEvent == nullptr)
	{
		const  DWORD errorCode = GetLastError();
		return MakeApiFailure("CreateEventW", HRESULT_FROM_WIN32(errorCode));
	}

	// Move ownership of the created resources to member variables
	m_commandQueue = std::move(commandQueue);
	m_fence = std::move(fence);
	m_fenceEvent = fenceEvent;

	m_nextFenceValue = 1;
	m_syncFailed = false;

	OutputDebugStringW(L"[101Renderer] CommandQueue initialized.\n");

	return {};
}

void CommandQueue::Finalize() noexcept
{
	if (m_fenceEvent != nullptr)
	{
		CloseHandle(m_fenceEvent);
		m_fenceEvent = nullptr;
	}

	m_fence.Reset();
	m_commandQueue.Reset();

	m_nextFenceValue = 1;
	m_syncFailed = false;

	OutputDebugStringW(L"[101Renderer] CommandQueue finalized.\n");
}

Result CommandQueue::Signal(std::uint64_t& outFenceValue)
{
	outFenceValue = 0;

	if (!IsInitialized())
	{
		return { false, "Signal : CommandQueue is not initialized." };
	}

	if (m_syncFailed)
	{
		return { false, "Signal : CommandQueue synchronization is unusable." };
	}

	// Reserve UINT64_MAX for device-removal detection.
	if (m_nextFenceValue == std::numeric_limits<uint64_t>::max())
	{
		return { false, "Signal : CommandQueue fence value is exhausted." };
	}

	// Signal the fence with the next fence value
	const uint64_t fenceValue = m_nextFenceValue;
	const HRESULT hr = m_commandQueue->Signal(m_fence.Get(), fenceValue);

	if (FAILED(hr))
	{
		m_syncFailed = true;
		return MakeApiFailure("ID3D12CommandQueue::Signal", hr);
	}

	++m_nextFenceValue;
	outFenceValue = fenceValue;

	return {};
}

Result CommandQueue::WaitForFence(uint64_t fenceValue)
{
	if (!IsInitialized())
	{
		return { false, "WaitForFence : CommandQueue is not initialized." };
	}

	if (m_syncFailed)
	{
		return { false, "WaitForFence : CommandQueue synchronization is unusable." };
	}

	if (fenceValue == 0 || fenceValue >= m_nextFenceValue)
	{
		return { false, "WaitForFence : Invalid fence value." };
	}

	// Get the value of the fence that has been completed so far.
	uint64_t completeValue = m_fence->GetCompletedValue();

	// Check for device-removed error
	constexpr uint64_t DeviceRemovedFenceValue = std::numeric_limits<uint64_t>::max();

	if (completeValue == DeviceRemovedFenceValue)
	{
		m_syncFailed = true;
		return { false, "WaitForFence : Device-Removed error detected." };
	}

	if (completeValue >= fenceValue)
	{
		// The fence has already been completed.
		return {};
	}

	// Wait for the fence to reach the specified value.
	const HRESULT hr = m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);

	if (FAILED(hr))
	{
		m_syncFailed = true;
		return MakeApiFailure("ID3D12Fence::SetEventOnCompletion", hr);
	}

	// Wait indefinitely for the fence event to be signaled.
	const DWORD waitResult = WaitForSingleObject(m_fenceEvent, INFINITE);

	if (waitResult == WAIT_FAILED)
	{
		const DWORD errorCode = GetLastError();
		m_syncFailed = true;

		return MakeApiFailure("WaitForSingleObject", HRESULT_FROM_WIN32(errorCode));
	}

	if (waitResult != WAIT_OBJECT_0)
	{
		m_syncFailed = true;
		return { false, "WaitForFence : Unexpected wait result." };
	}

	// Get the completed fence value after waiting.
	completeValue = m_fence->GetCompletedValue();

	if (completeValue == DeviceRemovedFenceValue)
	{
		m_syncFailed = true;
		return { false, "WaitForFence : Device-Removed error detected after waiting." };
	}

	if (completeValue < fenceValue)
	{
		m_syncFailed = true;
		return { false, "WaitForFence : Fence event was signaled before completion." };
	}

	return {};
}

Result CommandQueue::Flush()
{
	uint64_t fenceValue = 0;

	const Result signalResult = Signal(fenceValue);

	// If signaling the fence failed, return the error result.
	if (!signalResult.IsSuccess())
	{
		return signalResult;
	}

	// Wait for the previously signaled fence to be completed.
	// The thread will be blocked in this method until the fence is signaled.
	// (All commands submitted to the queue before the signal will be completed.)
	return WaitForFence(fenceValue);
}

QueueSubmissionResult CommandQueue::Execute(const CommandContext& context)
{
	if (!IsInitialized())
	{
		return { { false, "Execute : CommandQueue is not initialized." } };
	}

	if (m_syncFailed)
	{
		return { { false, "Execute : CommandQueue synchronization is unusable." } };
	}

	if (!context.IsClosed())
	{
		return { { false, "Execute : CommandContext must be closed." } };
	}

	// Get the command list from the context.
	ID3D12GraphicsCommandList* commandList = context.GetNativeCommandList();

	if (commandList == nullptr)
	{
		return { { false, "Execute : CommandList is null." } };
	}

	// Both the command list and the command queue must be of the same type.
	if (commandList->GetType() != m_commandQueue->GetDesc().Type)
	{
		return { { false, "Execute : CommandList and Queue types do not match." } };
	}

	// Check if the next fence value is exhausted before executing the command list.
	if (m_nextFenceValue == std::numeric_limits<uint64_t>::max())
	{
		return { { false, "Execute : CommandQueue fence value is exhausted." } };
	}

	ID3D12CommandList* commandLists[] = { commandList };

	QueueSubmissionResult submission{};

	// Execute the command list on the command queue.
	m_commandQueue->ExecuteCommandLists(1, commandLists);
	submission.executionIssued = true;

	// Signal the fence after executing the command list.
	submission.status = Signal(submission.fenceValue);

	return submission;
}

Result CommandQueue::QueryFenceCompletion(std::uint64_t fenceValue, bool& outCompleted)
{
	outCompleted = false;

	if (!IsInitialized())
	{
		return { false, "QueryFenceCompletion : CommandQueue is not initialized." };
	}

	if (m_syncFailed)
	{
		return { false, "QueryFenceCompletion : CommandQueue synchronization is unusable." };
	}

	if (fenceValue == 0 || fenceValue == invalidFenceValue || fenceValue >= m_nextFenceValue)
	{
		return { false, "QueryFenceCompletion : Invalid fence value." };
	}

	std::uint64_t completedValue = 0;
	Result result = QueryCompletedFenceValue(completedValue);

	if (!result.IsSuccess())
	{
		return result;
	}

	outCompleted = (completedValue >= fenceValue);
	return {};
}

Result CommandQueue::QueryCompletedFenceValue(std::uint64_t& outValue)
{
	outValue = 0;

	if (!IsInitialized())
	{
		return { false, "QueryCompletedFenceValue : CommandQueue is not initialized." };
	}

	if (m_syncFailed)
	{
		return { false, "QueryCompletedFenceValue : CommandQueue synchronization is unusable." };
	}

	const auto completedValue = m_fence->GetCompletedValue();

	if (completedValue == invalidFenceValue)
	{
		m_syncFailed = true;
		return { false, "QueryCompletedFenceValue: Device removal detected.", FailureKind::DeviceLost };
	}

	outValue = completedValue;
	return {};
}

}