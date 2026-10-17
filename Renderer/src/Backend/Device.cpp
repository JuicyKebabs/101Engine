#include "Device.h"
#include "ApiResult.h"

#include <Windows.h>
#include <d3d12sdklayers.h>

#include <utility>

namespace rendering::backend
{

namespace
{

using Microsoft::WRL::ComPtr;

// Enable or disable the DirectX 12 debug layer based on the provided configuration.
Result ConfigureDebugLayer(const RendererConfiguration& configuration)
{
	ComPtr<ID3D12Debug4> debugController;

	// Attempt to retrieve the debug interface for DirectX 12.
	const HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));

	if (FAILED(hr))
	{// Failed to get the debug interface.
		if (!configuration.enableDebugLayer)
		{
			return {};
		}

		// Return error result if it's required to enable the debug layer
		return MakeApiFailure("D3D12GetDebugInterface", hr);
	}

	if (configuration.enableDebugLayer)
	{
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(configuration.enableGpuValidation ? TRUE : FALSE);
	}
	else
	{
		debugController->DisableDebugLayer();
	}

	return {};
}

}

Result Device::Initialize(const RendererConfiguration& configuration)
{
	// Avoiding reinitialization.
	if (IsInitialized())
	{
		return Result{ false, "Device is already initialized." };
	}

	// Gpu validation requires the debug layer to be enabled.
	if (configuration.enableGpuValidation && !configuration.enableDebugLayer)
	{
		return Result{ false, "GPU validation requires the debug layer." };
	}

	// Configure the debug layer based on the provided configuration.
	const Result debugResult = ConfigureDebugLayer(configuration);

	if (!debugResult.IsSuccess())
	{
		return debugResult;
	}

	// Create all resources in local scope to ensure proper cleanup in case of failure.
	ComPtr<IDXGIFactory4> factory;
	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<ID3D12Device> device;

	// Create the DXGI factory
	const UINT factoryFlags = configuration.enableDebugLayer ? DXGI_CREATE_FACTORY_DEBUG : 0;

	HRESULT hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory));

	if (FAILED(hr))
	{
		return MakeApiFailure("CreateDXGIFactory2", hr);
	}

	// Enumerate the first adapter (index 0) from the factory.
	hr = factory->EnumAdapters1(0, adapter.GetAddressOf());

	if (FAILED(hr))
	{
		return MakeApiFailure("EnumAdapters1", hr);
	}

	constexpr D3D_FEATURE_LEVEL featureLevels[]
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	for (const D3D_FEATURE_LEVEL featureLevel : featureLevels)
	{
		// Attempt to create the DirectX 12 device with the current feature level.
		hr = D3D12CreateDevice(adapter.Get(), featureLevel, IID_PPV_ARGS(&device));

		if (SUCCEEDED(hr))
		{
			break; // Successfully created the device, exit the loop.
		}

		const Result attempt = MakeApiFailure("D3D12CreateDevice attempt", hr);

		OutputDebugStringA(attempt.message.c_str());
		OutputDebugStringA("\n");
	}

	// Failed tp create the device with all specified feature levels.
	if (FAILED(hr))
	{
		return MakeApiFailure("D3D12CreateDevice", hr);
	}

	// Move ownership of all created resources to member variables
	m_factory = std::move(factory);
	m_adapter = std::move(adapter);
	m_device = std::move(device);

	OutputDebugStringW(L"[101Renderer] Dx12Device initialized.\n");

	return {};
}

void Device::Finalize() noexcept
{
	m_device.Reset();
	m_adapter.Reset();
	m_factory.Reset();
}

Result Device::CheckStatus() const
{
	if (!m_device)
	{
		return { false, "Device is not initialized." };
	}

	const HRESULT hr = m_device->GetDeviceRemovedReason();

	if (FAILED(hr))
	{
		Result result = MakeApiFailure("GetDeviceRemovedReason", hr);
		result.failureKind = FailureKind::DeviceLost;
		return result;
	}

	return {};
}

}