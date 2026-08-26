#pragma once
#include <vector>
#include <cstddef>
#include <cstdint>
#include "Engine/Graphics/CommandQueue.h"
#include "Engine/Graphics/CommandContext.h"

class FrameCommandManager
{
public:
	bool Initialize(ID3D12Device* pDevice, size_t frameCount);

	ID3D12GraphicsCommandList* BeginFrame(size_t frameIndex);	// Prepare a frame slot for command recording
	bool  EndFrame(size_t frameIndex);							// Finalize and submit the recorded frame commands

	bool Flush();

	ID3D12CommandQueue* GetNativeQueue() const { return m_commandQueue.GetNativeQueue(); }
	ID3D12GraphicsCommandList* GetCommandList(size_t frameIndex) const
	{
		if (frameIndex >= m_frameSlots.size())
		{
			return nullptr;
		}
		return m_frameSlots[frameIndex].commandContext.GetCommandList();
	}

private:
	// Slot for each frame buffer, containing the command context and its state
	struct FrameSlot
	{
		CommandContext commandContext;							// Command context for this frame slot
		uint64_t submissionFenceValue = 0;						// The last submitted fence value for this frame slot
	};

	CommandQueue m_commandQueue;
	std::vector<FrameSlot> m_frameSlots;

	bool m_isUsable = false;	// This flag is set to false when a problem occurs that is not caused by a specific context.
};