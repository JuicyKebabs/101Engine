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

	enum class Format {
		UNKNOWN = 0,
		RGBA8,
		RGBA16F,
		RGBA32F,
		R16F,
		R32F,
		R24G8_TYPELESS,
	};

	static DXGI_FORMAT ConvertToDXGIFormat(Format format) {
		switch (format)
		{
		case GpuTexture::Format::UNKNOWN:
			return DXGI_FORMAT_UNKNOWN;
			break;
		case GpuTexture::Format::RGBA8:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case GpuTexture::Format::RGBA16F:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
			break;
		case GpuTexture::Format::RGBA32F:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
			break;
		case GpuTexture::Format::R16F:
			return DXGI_FORMAT_R16_FLOAT;
			break;
		case GpuTexture::Format::R32F:
			return DXGI_FORMAT_R32_FLOAT;
			break;
		case GpuTexture::Format::R24G8_TYPELESS:
			return DXGI_FORMAT_R24G8_TYPELESS;
			break;
		default:
			assert(false && "RenderTargetTexture: Invalid color format");
			return DXGI_FORMAT_UNKNOWN;
			break;
		}
	}
	
	enum class DepthFormat {
		UNKNOWN = 0,
		DEPTH16F,
		DEPTH24F,
		DEPTH32F,
	};

	static DXGI_FORMAT ConvertToDXGIDepthFormat(DepthFormat format) {
		switch (format)
		{
		case GpuTexture::DepthFormat::UNKNOWN:
			return DXGI_FORMAT_UNKNOWN;
			break;
		case GpuTexture::DepthFormat::DEPTH16F:
			return DXGI_FORMAT_D16_UNORM;
			break;
		case GpuTexture::DepthFormat::DEPTH24F:
			return DXGI_FORMAT_D24_UNORM_S8_UINT;
			break;
		case GpuTexture::DepthFormat::DEPTH32F:
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

		Format format = Format::RGBA16F;
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

		DepthFormat depthFormat = DepthFormat::DEPTH32F;
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

	ID3D12Resource* GetResource() const { return m_pResource.Get(); }
	ResourceState GetState() const { return m_currentState; }
	Format GetFormat() const { return m_format; }
	float GetWidth() const { return m_width; }
	float GetHeight() const { return m_height; }
	const float* GetClearColor() const { return m_clearColor; }
	float GetClearDepth() const { return m_clearDepth; }

	uint32_t GetRtvIndex() { assert(m_rtvIndex != UINT32_MAX && "GpuTexture: RTV index is not set"); return m_rtvIndex; }
	uint32_t GetDsvIndex() { assert(m_dsvIndex != UINT32_MAX && "GpuTexture: DSV index is not set"); return m_dsvIndex; }
	uint32_t GetSrvIndex() { assert(m_srvIndex != UINT32_MAX && "GpuTexture: SRV index is not set"); return m_srvIndex; }
	uint32_t GetUavIndex() { assert(m_uavIndex != UINT32_MAX && "GpuTexture: UAV index is not set"); return m_uavIndex; }

	void MarkAsRenderTarget() { m_currentState = ResourceState::RenderTarget; }
	void MarkAsDepthWrite() { m_currentState = ResourceState::DepthWrite; }
	void MarkAsShaderResource() { m_currentState = ResourceState::ShaderResource; }
	void MarkAsUnorderedAccess() { m_currentState = ResourceState::UnorderedAccess; }

private:
	ComPtr<ID3D12Resource> m_pResource = nullptr;

	uint32_t m_rtvIndex = UINT32_MAX;
	uint32_t m_dsvIndex = UINT32_MAX;
	uint32_t m_srvIndex = UINT32_MAX;
	uint32_t m_uavIndex = UINT32_MAX;

	UINT m_width = 0.0f;
	UINT m_height = 0.0f;

	ResourceState m_currentState = ResourceState::Common;

	Format m_format = Format::RGBA16F;
	float m_clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	DepthFormat m_depthFormat = DepthFormat::DEPTH32F;
	float m_clearDepth = 1.0f;
};