#pragma once
#include <d3d12.h>
#include <DirectXTex.h>
#include <cstdint>
#include <string>
#include <assert.h>
#include "Engine/Core/ComPtr/ComPtr.h"

using TextureHandle = uint32_t;
using SrvIndex = uint32_t;

constexpr TextureHandle InvalidTextureHandle = UINT32_MAX;
constexpr SrvIndex InvalidSrvIndex = UINT32_MAX;

class Texture
{
public:
	enum class State : uint8_t {
		Unloaded,
		Pending,
		Ready,
		Failed
	};

	struct InitDesc {
		TextureHandle handle = InvalidTextureHandle;
		SrvIndex srvIndex = InvalidSrvIndex;
		std::wstring path;
		DirectX::TexMetadata meta = {};
	};

public:
	Texture() = default;
	explicit Texture(ComPtr<ID3D12Resource> resource, const InitDesc& desc) { Initialize(resource, desc); }
	~Texture() = default;

	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	void Initialize(ComPtr<ID3D12Resource> resource, const InitDesc& desc)
	{
		assert(resource != nullptr && "Texture: Resource cannot be null");
		m_pResource = resource;
		m_handle = desc.handle;
		m_srvIndex = desc.srvIndex;
		m_path = desc.path;
		m_meta = desc.meta;
		m_state = State::Unloaded;
	}
	void MarkAsPending() { m_state = State::Pending; }
	void MarkAsReady() { m_state = State::Ready; }
	void MarkAsFailed() { m_state = State::Failed; }

	ID3D12Resource* GetResource() const { return m_pResource.Get(); }
	TextureHandle GetHandle() const { return m_handle; }
	SrvIndex GetSrvIndex() const { return m_srvIndex; }
	const std::wstring& GetPath() const { return m_path; }
	State GetState() const { return m_state; }
	const DirectX::TexMetadata& GetMeta() const { return m_meta; }

private:
	ComPtr<ID3D12Resource> m_pResource;
	TextureHandle m_handle = InvalidTextureHandle;
	SrvIndex m_srvIndex = InvalidSrvIndex;
	std::wstring m_path;
	State m_state = State::Unloaded;
	DirectX::TexMetadata m_meta = {};
};