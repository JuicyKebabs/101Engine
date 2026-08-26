#include <d3dx12.h>
#include "CommandQueue.h"
#include "Engine/Core/Debug/Debug.h"

CommandQueue::~CommandQueue()
{
	if (m_fenceEvent)
	{
		CloseHandle(m_fenceEvent);
		m_fenceEvent = nullptr;
	}
}

bool CommandQueue::Initialize(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type)
{
	if (!pDevice)
	{
		DBG("CommandQueue::Initialize: Invalid device pointer.");
		return false;
	}

	HRESULT hr;
	 
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = type;

	// Create command queue
	hr = pDevice->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&m_pCommandQueue));
	if (FAILED(hr))
	{
		DBG("CommandQueue::Initialize: Failed to create command queue.");
		return false;
	}

	// Create fence with initial value of 0
	hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence));
	if (FAILED(hr))
	{
		DBG("CommandQueue::Initialize: Failed to create fence.");
		return false;
	}

	// Create an event for fence synchronization
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!m_fenceEvent)
	{
		DBG("CommandQueue::Initialize: Failed to create fence event.");
		return false;
	}

	m_type = type;
	m_nextFenceValue = 1;	// Start fence values from 1

	return true;
}

uint64_t CommandQueue::Execute(ID3D12CommandList* pCommandList)
{
	if (!m_pCommandQueue || !m_pFence)
	{
		DBG("CommandQueue::Execute: Command queue is not initialized.");
		return 0;
	}

	if (!pCommandList)
	{
		DBG("CommandQueue::Execute: Invalid command list pointer.");
		return 0;
	}

	// Execute the command list
	ID3D12CommandList* cmdLists[] = { pCommandList };
	m_pCommandQueue->ExecuteCommandLists(1, cmdLists);

	// Signal the command queue and return the fence value
	// This fence value means the end of this command list execution.
	return Signal();
}

uint64_t CommandQueue::Signal()
{
	if (!m_pCommandQueue || !m_pFence)
	{
		DBG("CommandQueue::Signal: Command queue or fence is not initialized.");
		return 0;
	}

	const uint64_t fenceValue = m_nextFenceValue;

	// Signal the command queue with the next fence value.
	HRESULT hr = m_pCommandQueue->Signal(m_pFence.Get(), fenceValue);

	if (FAILED(hr))
	{
		DBG("CommandQueue::Signal: Failed to signal command queue.");
		return 0;
	}

	m_nextFenceValue++;	// Increment the next fence value for future signals.

	return fenceValue;
}

bool CommandQueue::IsFenceComplete(uint64_t fenceValue) const
{
	if (!m_pFence)
	{
		DBG("CommandQueue::IsFenceComplete: Fence is not initialized.");
		return false;
	}

	// Get the completed fence value from the fence object.
	const uint64_t completedValue = m_pFence->GetCompletedValue();

	if (completedValue == UINT64_MAX)
	{
		DBG("CommandQueue::IsFenceComplete: Fence has been completed with an invalid value.");
		return false;
	}

	// Greater value means the fence has been completed.
	return completedValue >= fenceValue;
}

bool CommandQueue::WaitForFence(uint64_t fenceValue)
{
	if (!m_pFence || !m_fenceEvent)
	{
		DBG("CommandQueue::WaitForFence: Fence or fence event is not initialized.");
		return false;
	}

	// Get the completed fence value from the fence object.
	const uint64_t completedValue = m_pFence->GetCompletedValue();

	if (completedValue == UINT64_MAX)
	{
		DBG("CommandQueue::WaitForFence: Fence has been completed with an invalid value.");
		return false;
	}

	if (completedValue >= fenceValue)
	{// Fence has already been completed, no need to wait.
		return true;
	}

	// Set the event to be signaled when the fence reaches the specified value.
	const HRESULT hr = m_pFence->SetEventOnCompletion(fenceValue, m_fenceEvent);

	if (FAILED(hr))
	{
		DBG("CommandQueue::WaitForFence: Failed to set event on fence completion.");
		return false;
	}
	
	// Wait for the fence event to be signaled.
	const DWORD waitResult = WaitForSingleObject(m_fenceEvent, INFINITE);

	if (waitResult != WAIT_OBJECT_0)
	{
		DBG("CommandQueue::WaitForFence: Wait for fence event failed.");
		return false;
	}

	// Get the completed fence value again after waiting to check if it has been completed successfully.
	uint64_t afterWaitCompletedValue = m_pFence->GetCompletedValue();

	// Guard against the case where the fence value is still not completed after waiting
	// due to some unexpected issue. This should not happen, but we check for it just in case.
	if (afterWaitCompletedValue == UINT64_MAX || afterWaitCompletedValue < fenceValue)
	{
		DBG("CommandQueue::WaitForFence: Fence has been completed with an invalid value after waiting.");
		return false;
	}

	return true;
}

bool CommandQueue::Flush()
{
	// Signal the command queue and get the fence value for the current state.
	const uint64_t fenceValue = Signal();

	if (fenceValue == 0)
	{
		DBG("CommandQueue::Flush: Failed to signal command queue.");
		return false;
	}

	// Wait for the fence to be completed, ensuring all commands have finished executing.
	return WaitForFence(fenceValue);
}