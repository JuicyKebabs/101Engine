#pragma once
#include <d3d12.h>
#include "Engine/Core/ComPtr/ComPtr.h"

class CommandContext
{
public:
	// State of the command context
	enum class State
	{
		Uninitialized,	// Before Initialize() is called
		Ready,			// Ready to begin recording commands
		Recording,		// Currently recording commands
		Unusable		// Cannot be used due to an error
	};

public:
	CommandContext() = default;
	CommandContext(const CommandContext&) = delete;
	CommandContext& operator=(const CommandContext&) = delete;
	CommandContext(CommandContext&&) noexcept = default;
	CommandContext& operator=(CommandContext&&) noexcept = default;

	bool Initialize(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type);

	bool Begin();	// Begin recording commands
	bool Close();	// Close the command list

	ID3D12GraphicsCommandList* GetCommandList() const { return m_pCommandList.Get(); }

	bool IsReady() const { return m_state == State::Ready; }
	bool IsRecording() const { return m_state == State::Recording; }

private:

	ComPtr<ID3D12GraphicsCommandList> m_pCommandList;
	ComPtr<ID3D12CommandAllocator> m_pCommandAllocator;

	D3D12_COMMAND_LIST_TYPE m_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	
	State m_state = State::Uninitialized;
};