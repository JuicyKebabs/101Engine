#pragma once

#include "Renderer/RendererConfiguration.h"
#include "Renderer/Result.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

//----------------------------------------------------------------------------------
// Device class
// Represents a DirectX 12 device and its associated resources. 
// It encapsulates the creation and management of the device, factory, and adapter.
//----------------------------------------------------------------------------------

namespace rendering::backend
{

class Device
{
public:
	Device() = default;
	
	Device(const Device&) = delete;
	Device& operator=(const Device&) = delete;
	Device(Device&&) = delete;
	Device& operator=(Device&&) = delete;

	Result Initialize(const RendererConfiguration& configuration);
	void Finalize() noexcept;

	[[nodiscard]]
	bool IsInitialized() const { return m_device != nullptr; }
	
	ID3D12Device* GetNativeDevice() const noexcept { return m_device.Get(); }
	IDXGIFactory4* GetNativeFactory() const noexcept { return m_factory.Get(); }

	Result CheckStatus() const;

private:
	Microsoft::WRL::ComPtr<IDXGIFactory4> m_factory;
	Microsoft::WRL::ComPtr<IDXGIAdapter1> m_adapter;
	Microsoft::WRL::ComPtr<ID3D12Device> m_device;
};

}