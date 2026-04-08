#pragma once
#include <d3d12.h>
#include <d3dx12.h>
#include <assert.h>
#include "Engine/Core/ComPtr/ComPtr.h"

class RenderTargetTexture
{
public:
	enum class State{
		Write,	// Render target is being written to (rendering)
		Read,	// Render target is being read from (post-processing)
	};

	static D3D12_RESOURCE_STATES ConvertToD3D12State(State state) {
		switch (state)
		{
		case RenderTargetTexture::State::Write:
			return D3D12_RESOURCE_STATE_RENDER_TARGET;
			break;
		case RenderTargetTexture::State::Read:
			return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			break;
		default:
			assert(false && "RenderTargetTexture: Invalid state");
			return D3D12_RESOURCE_STATE_COMMON;
			break;
		}
	}

	enum class Format {
		RGBA8,
		RGBA16F,
	};

	static DXGI_FORMAT ConvertToDXGIFormat(Format format) {
		switch (format)
		{
		case RenderTargetTexture::Format::RGBA8:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case RenderTargetTexture::Format::RGBA16F:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
			break;
		default:
			assert(false && "RenderTargetTexture: Invalid format");
			return DXGI_FORMAT_UNKNOWN;
			break;
		}
	}

	struct InitDesc {
		uint32_t m_rtvIndex = UINT32_MAX;
		uint32_t m_srvIndex = UINT32_MAX;
		State state = State::Write;
		Format format = Format::RGBA16F;
		UINT width = 0.0f;
		UINT height = 0.0f;
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	};

public:
	RenderTargetTexture() = default;
	~RenderTargetTexture() = default;

	void Initialize(ComPtr<ID3D12Resource> resource, const InitDesc& desc) {
		assert(resource != nullptr && "RenderTargetTexture: Resource cannot be null");
		assert(desc.m_rtvIndex != UINT32_MAX && "RenderTargetTexture: RTV index must be valid");
		assert(desc.m_srvIndex != UINT32_MAX && "RenderTargetTexture: SRV index must be valid");
		m_pResource = resource;
		m_rtvIndex = desc.m_rtvIndex;
		m_srvIndex = desc.m_srvIndex;
		m_currentState = desc.state;
		m_format = desc.format;
		m_width = desc.width;
		m_height = desc.height;
		m_clearColor[0] = desc.clearColor[0];
		m_clearColor[1] = desc.clearColor[1];
		m_clearColor[2] = desc.clearColor[2];
		m_clearColor[3] = desc.clearColor[3];
	}

	ID3D12Resource* GetResource() const { return m_pResource.Get(); }
	uint32_t GetRtvIndex() const { return m_rtvIndex; }
	uint32_t GetSrvIndex() const { return m_srvIndex; }
	State GetState() const { return m_currentState; }
	Format GetFormat() const { return m_format; }
	float GetWidth() const { return m_width; }
	float GetHeight() const { return m_height; }
	const float* GetClearColor() const { return m_clearColor; }

	void MarkAsRead() { m_currentState = State::Read; }
	void MarkAsWrite() { m_currentState = State::Write; }

private:
	ComPtr<ID3D12Resource> m_pResource = nullptr;
	uint32_t m_rtvIndex = UINT32_MAX;
	uint32_t m_srvIndex = UINT32_MAX;
	State m_currentState = State::Write;
	Format m_format = Format::RGBA16F;
	UINT m_width = 0.0f;
	UINT m_height = 0.0f;
	float m_clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
};