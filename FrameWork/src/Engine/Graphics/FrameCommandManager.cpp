#include "FrameCommandManager.h"
#include "Engine/Core/Debug/Debug.h"

bool FrameCommandManager::Initialize(ID3D12Device* pDevice, size_t frameCount)
{
	if (!pDevice || frameCount == 0)
	{
		DBG("FrameCommandManager::Initialize: Invalid parameters.");
		return false;
	}

	if (!m_commandQueue.Initialize(pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT))
	{
		DBG("FrameCommandManager::Initialize: Failed to initialize command queue.");
		return false;
	}

	m_frameSlots.resize(frameCount);

	for (auto& frameSlot : m_frameSlots)
	{
		if (!frameSlot.commandContext.Initialize(pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT))
		{
			DBG("FrameCommandManager::Initialize: Failed to initialize command context.");
			return false;
		}
	}

	m_isUsable = true;

	return true;
}

ID3D12GraphicsCommandList* FrameCommandManager::BeginFrame(size_t frameIndex)
{
	if (!m_isUsable)
	{
		DBG("FrameCommandManager::BeginFrame: FrameCommandManager is not usable.");
		return nullptr;
	}

	if (frameIndex >= m_frameSlots.size())
	{
		DBG("FrameCommandManager::BeginFrame: Invalid frame index.");
		return nullptr;
	}

	FrameSlot& slot = m_frameSlots[frameIndex];

	// Wait for the GPU to finish executing the previous frame's commands if necessary
	// Check if the GPU signaled the fence value which indicates that the commands for previous frame have completed execution.
	if (slot.submissionFenceValue != 0)
	{
		if (!m_commandQueue.WaitForFence(slot.submissionFenceValue))
		{
			DBG("FrameCommandManager::BeginFrame: Failed to wait for fence.");
			m_isUsable = false;
			return nullptr;
		}
	}
	
	// Begin recording commands for this frame
	const bool began = slot.commandContext.Begin();

	if (!began)
	{
		DBG("FrameCommandManager::BeginFrame: Failed to begin command context.");
		return nullptr;
	}

	return slot.commandContext.GetCommandList();
}

bool FrameCommandManager::EndFrame(size_t frameIndex)
{
	if (!m_isUsable)
	{
		DBG("FrameCommandManager::EndFrame: FrameCommandManager is not usable.");
		return false;
	}

	if (frameIndex >= m_frameSlots.size())
	{
		DBG("FrameCommandManager::EndFrame: Invalid frame index.");
		return false;
	}

	FrameSlot& slot = m_frameSlots[frameIndex];

	// Close the command list for this frame
	if (!slot.commandContext.Close())
	{
		DBG("FrameCommandManager::EndFrame: Failed to close command list.");
		return false;
	}

	// Execute the command list and get the fence value 
	// which will be signaled when the GPU has finished executing the commands
	const uint64_t fenceValue = m_commandQueue.Execute(slot.commandContext.GetCommandList());
	if (fenceValue == 0)
	{
		DBG("FrameCommandManager::EndFrame: Failed to execute command list.");
		m_isUsable = false;
		return false;
	}

	slot.submissionFenceValue = fenceValue;

	return true;
}

bool FrameCommandManager::Flush()
{
	if (!m_isUsable)
	{
		DBG("FrameCommandManager::Flush: FrameCommandManager is not usable.");
		return false;
	}

	if (!m_commandQueue.Flush())
	{
		DBG("FrameCommandManager::Flush: Failed to flush command queue.");
		m_isUsable = false;
		return false;
	}

	return true;
}