#pragma once
#include <d3d12.h>
#include <d3dx12.h>
#include <assert.h>
#include "Engine/Core/ComPtr/ComPtr.h"
#include "Engine//Graphics/DescriptorHeapAllocator.h"

class GpuTexture
{
public:
	enum class ResourceState{
		Common,
		RenderTarget,
		DepthWrite,
		ShaderResource,
		UnorderedAccess,
	};

	static D3D12_RESOURCE_STATES ConvertToD3D12State(ResourceState state) {
		switch (state)
		{
		case GpuTexture::ResourceState::Common:
			return D3D12_RESOURCE_STATE_COMMON;
			break;
		case GpuTexture::ResourceState::RenderTarget:
			return D3D12_RESOURCE_STATE_RENDER_TARGET;
			break;
		case GpuTexture::ResourceState::DepthWrite:
			return D3D12_RESOURCE_STATE_DEPTH_WRITE;
			break;
		case GpuTexture::ResourceState::ShaderResource:
			return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			break;
		case GpuTexture::ResourceState::UnorderedAccess:
			return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			break;
		default:
			assert(false && "RenderTargetTexture: Invalid state");
			return D3D12_RESOURCE_STATE_COMMON;
			break;
		}
	}

	enum class ColorFormat {
		UNKNOWN = 0,
		RGBA8,
		RGBA16F,
		RGBA32F,
		R32F,
		R16_UNORM,
		R24_UNORM_X8_TYPELESS,
	};

	static DXGI_FORMAT ConvertToDXGIColorFormat(ColorFormat format) {
		switch (format)
		{
		case GpuTexture::ColorFormat::UNKNOWN:
			return DXGI_FORMAT_UNKNOWN;
			break;
		case GpuTexture::ColorFormat::RGBA8:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case GpuTexture::ColorFormat::RGBA16F:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
			break;
		case GpuTexture::ColorFormat::RGBA32F:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
			break;
		case GpuTexture::ColorFormat::R32F:
			return DXGI_FORMAT_R32_FLOAT;
			break;
		case GpuTexture::ColorFormat::R16_UNORM:
			return DXGI_FORMAT_R16_UNORM;
			break;
		case GpuTexture::ColorFormat::R24_UNORM_X8_TYPELESS:
			return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			break;
		default:
			assert(false && "RenderTargetTexture: Invalid color format");
			return DXGI_FORMAT_UNKNOWN;
			break;
		}
	}
	
	enum class DepthFormat {
		UNKNOWN = 0,
		D16_UNORM,
		D24F,
		D32F,
	};

	static DXGI_FORMAT ConvertToDXGIDepthFormat(DepthFormat format) {
		switch (format)
		{
		case GpuTexture::DepthFormat::UNKNOWN:
			return DXGI_FORMAT_UNKNOWN;
			break;
		case GpuTexture::DepthFormat::D16_UNORM:
			return DXGI_FORMAT_D16_UNORM;
			break;
		case GpuTexture::DepthFormat::D24F:
			return DXGI_FORMAT_D24_UNORM_S8_UINT;
			break;
		case GpuTexture::DepthFormat::D32F:
			return DXGI_FORMAT_D32_FLOAT;
			break;
		default:
			assert(false && "RenderTargetTexture: Invalid depth format");
			return DXGI_FORMAT_UNKNOWN;
			break;
		}
	}

	struct InitDesc {
		UINT width = 0.0f;
		UINT height = 0.0f;

		ResourceState initialState = ResourceState::Common;

		ColorFormat format = ColorFormat::RGBA16F;
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

		DepthFormat depthFormat = DepthFormat::UNKNOWN;
		float clearDepth = 1.0f;

		bool useRTV = false;
		bool useDSV = false;
		bool useSRV = false;
		bool useUAV = false;
	};

public:
	GpuTexture() = default;
	~GpuTexture() = default;

	void Initialize(ID3D12Device* device, DescriptorHeapAllocator* allocator, const InitDesc& desc);
	void TransitionToState(ID3D12GraphicsCommandList* cmdList, ResourceState newState);

	ID3D12Resource* GetResource() const { return m_pResource.Get(); }
	ResourceState GetState() const { return m_currentState; }
	ColorFormat GetFormat() const { return m_colorFormat; }
	float GetWidth() const { return m_width; }
	float GetHeight() const { return m_height; }
	const float* GetClearColor() const { return m_clearColor; }
	float GetClearDepth() const { return m_clearDepth; }

	uint32_t GetRtvIndex() { assert(m_rtvIndex != UINT32_MAX && "GpuTexture: RTV index is not set"); return m_rtvIndex; }
	uint32_t GetDsvIndex() { assert(m_dsvIndex != UINT32_MAX && "GpuTexture: DSV index is not set"); return m_dsvIndex; }
	uint32_t GetSrvIndex() { assert(m_srvIndex != UINT32_MAX && "GpuTexture: SRV index is not set"); return m_srvIndex; }
	uint32_t GetUavIndex() { assert(m_uavIndex != UINT32_MAX && "GpuTexture: UAV index is not set"); return m_uavIndex; }

private:
	ComPtr<ID3D12Resource> m_pResource = nullptr;

	uint32_t m_rtvIndex = UINT32_MAX;
	uint32_t m_dsvIndex = UINT32_MAX;
	uint32_t m_srvIndex = UINT32_MAX;
	uint32_t m_uavIndex = UINT32_MAX;

	UINT m_width = 0.0f;
	UINT m_height = 0.0f;

	ResourceState m_currentState = ResourceState::Common;

	ColorFormat m_colorFormat = ColorFormat::RGBA16F;
	float m_clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	DepthFormat m_depthFormat = DepthFormat::D32F;
	float m_clearDepth = 1.0f;
};