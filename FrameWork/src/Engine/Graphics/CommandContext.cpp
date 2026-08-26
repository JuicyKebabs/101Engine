#include "CommandContext.h"
#include "Engine/Core/Debug/Debug.h"

bool CommandContext::Initialize(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type)
{
	if (!pDevice)
	{
		DBG("CommandContext::Initialize: Invalid device pointer.");
		return false;
	}

	HRESULT hr;
	
	hr = pDevice->CreateCommandAllocator(type, IID_PPV_ARGS(&m_pCommandAllocator));

	if (FAILED(hr))
	{
		DBG("CommandContext::Initialize: Failed to create command allocator. HRESULT: 0x%08X", hr);
		return false;
	}

	hr = pDevice->CreateCommandList(0, type, m_pCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_pCommandList));

	if (FAILED(hr))
	{
		DBG("CommandContext::Initialize: Failed to create command list. HRESULT: 0x%08X", hr);
		return false;
	}

	// Close the command list initially; it will be reset when Begin() is called
	hr = m_pCommandList->Close();

	if (FAILED(hr))
	{
		DBG("CommandContext::Initialize: Failed to close command list. HRESULT: 0x%08X", hr);
		return false;
	}

	m_type = type;
	m_state = State::Ready;

	return true;
}

bool CommandContext::Begin()
{
	if (m_state != State::Ready)
	{
		DBG("CommandContext::Begin: Command context is not in a ready state.");
		return false;
	}

	if (!m_pCommandAllocator || !m_pCommandList)
	{
		DBG("CommandContext::Begin: Command allocator or command list is not initialized.");
		m_state = State::Unusable;
		return false;
	}

	HRESULT hr;
	
	hr = m_pCommandAllocator->Reset();

	if (FAILED(hr))
	{
		DBG("CommandContext::Begin: Failed to reset command allocator. HRESULT: 0x%08X", hr);
		m_state = State::Unusable;
		return false;
	}

	hr = m_pCommandList->Reset(m_pCommandAllocator.Get(), nullptr);

	if (FAILED(hr))
	{
		DBG("CommandContext::Begin: Failed to reset command list. HRESULT: 0x%08X", hr);
		m_state = State::Unusable;
		return false;
	}

	m_state = State::Recording;

	return true;
}

bool CommandContext::Close()
{
	if (m_state != State::Recording)
	{
		DBG("CommandContext::Close: Command context is not in a recording state.");
		return false;
	}

	if (!m_pCommandList)
	{
		DBG("CommandContext::Close: Command list is not initialized.");
		m_state = State::Unusable;
		return false;
	}

	HRESULT hr = m_pCommandList->Close();

	if (FAILED(hr))
	{
		DBG("CommandContext::Close: Failed to close command list. HRESULT: 0x%08X", hr);
		m_state = State::Unusable;
		return false;
	}

	m_state = State::Ready;

	return true;
}