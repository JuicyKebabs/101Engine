#pragma once
#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXTex.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include "Texture.h"
#include "Engine/Core/ComPtr/ComPtr.h"
#include "Engine/Graphics/DescriptorHeapAllocator.h"


// Pending texture upload structure
struct PendingTextureUpload
{
	TextureHandle textureHandle;						// Texture handle
	ComPtr<ID3D12Resource> uploadBuffer;				// Upload buffer
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;	// Subresource data array
	std::vector<uint8_t> ownedData;						// Owned data to keep alive during upload 
};

// Texture manager class
class TextureManager
{
public:
	TextureManager() {};	// Constructor
	~TextureManager() {};	// Destructor

	// Get singleton instance
	static TextureManager* GetInstance()
	{
		static TextureManager instance;
		return &instance;
	}

	// Copying is prohibited
	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	// Move semantics are allowed
	TextureManager(TextureManager&&) noexcept = default;
	TextureManager& operator=(TextureManager&&) noexcept = default;

	void Initialize(	// Initialization
		ID3D12Device* pDevice,	// Device
		DescriptorHeapAllocator* pDescriptorHeapAllocator	// Descriptor heap allocator
	);

	TextureHandle LoadTexture(	// Load SRV from file
		const std::wstring& path	// File path
	);

	SrvIndex CreateSrv(			// Create SRV for a texture resource
		ID3D12Resource* pResource,	// Texture resource
		DXGI_FORMAT format			// Texture format
	);

	void UploadPendingTextures(ID3D12GraphicsCommandList* cmdList);	// Upload pending textures

	Texture* GetTexture(TextureHandle handle) const;			// Get texture by handle
	SrvIndex GetTextureSrvIndex(TextureHandle handle) const;	// Get SRV index for a specific texture
	TextureHandle GetDefaultTextureSrvIndex() const;	// Get default texture index

private:
	// Texture management
	std::unordered_map<std::wstring, TextureHandle> m_loadedTextures;		// Map of loaded textures
	std::unordered_map<TextureHandle, std::unique_ptr<Texture>> m_textures;	// Map of texture handles to texture data
	std::vector<ComPtr<ID3D12Resource>> m_uploadKeepAlive;					// Keep-alive array for upload buffers
	std::vector<PendingTextureUpload> m_pendingUploads;						// Array of pending texture uploads
	TextureHandle m_nextTextureHandle = 0;									// Next texture handle to assign

	TextureHandle m_defaultTextureHandle = 0;	// Default texture handle (reserved index)

	ID3D12Device* m_pDevice = nullptr;								// Device
	DescriptorHeapAllocator* m_pDescriptorHeapAllocator = nullptr;	// Descriptor heap allocator for SRV management

private:
	void CreateDefaultTexture();	// Create default texture
};