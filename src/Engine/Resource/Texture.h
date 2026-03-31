#pragma once
#include <d3d12.h>
#include <cstdint>
#include "Engine/Core/ComPtr/ComPtr.h"

using TextureHandle = uint32_t;

class Texture
{
public:
	Texture() = default;
	~Texture() = default;

	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	ID3D12Resource* GetResource() const { return m_pResource.Get(); }
	TextureHandle GetHandle() const { return m_handle; }

private:
	ComPtr<ID3D12Resource> m_pResource;
	TextureHandle m_handle;
};