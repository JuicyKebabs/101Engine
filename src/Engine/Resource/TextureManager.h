#pragma once
#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXTex.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include "Engine/Core/ComPtr/ComPtr.h"

// Pending texture upload structure
struct PendingTextureUpload
{
	ComPtr<ID3D12Resource> texture;						// Texture resource
	ComPtr<ID3D12Resource> uploadBuffer;				// Upload buffer
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;	// Subresource data array
	std::vector<uint8_t> ownedData;						// Owned data to keep alive during upload 
	uint32_t srvIndex;									// SRV descriptor index
};

// Reserved SRV indices for special textures
enum class TEXTURE_SRV_INDEX_RESERVED : uint32_t
{
	POST_PROCESSING = 0,	// Reserved index for post-processing texture
	DEFAULT_TEXTURE,		// Reserved index for default white texture
	RESERVED_COUNT			// Count of reserved indices
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
		uint32_t maxDescriptors	// Maximum descriptor count
	);

	uint32_t LoadSrvFromFile(	// Load SRV from file
		const std::wstring& path	// File path
	);

	uint32_t AllocateSrv();	// Allocate SRV descriptor index (for manually created textures)
	void CreateSrv(			// Create SRV for a texture resource
		ID3D12Resource* pResource,	// Texture resource
		DXGI_FORMAT format,			// Texture format
		uint32_t srvIndex			// SRV descriptor index
	);

	void UploadPendingTextures(ID3D12GraphicsCommandList* cmdList);	// Upload pending textures

	ID3D12DescriptorHeap* GetSrvHeap() const;		// Get SRV heap (where SRVs are stored)
	UINT GetSrvIncrementSize() const;				// Get SRV descriptor increment size
	
	uint32_t GetPostProcessingTextureIndex() const;	// Get post-processing texture index
	uint32_t GetDefaultTextureIndex() const;	// Get default texture index

private:
	// SRV management
	ComPtr<ID3D12DescriptorHeap> m_pSrvHeap;	// SRV descriptor heap (where texture SRVs are stored)
	UINT m_srvIncrementSize = 0;															// SRV descriptor increment size
	UINT m_nextFreeIndex = static_cast<UINT>(TEXTURE_SRV_INDEX_RESERVED::RESERVED_COUNT);	// Next free SRV index

	// Texture management
	std::unordered_map<std::wstring, uint32_t> m_loadedTextures;	// Map of loaded textures
	std::vector<ComPtr<ID3D12Resource>> m_textures;					// Array of texture resources
	std::vector< ComPtr<ID3D12Resource> > m_uploadKeepAlive;		// Keep-alive array for upload buffers
	std::vector<PendingTextureUpload> m_pendingUploads;				// Array of pending texture uploads

	ID3D12Device* m_pDevice = nullptr;	// Device

private:
	void CreateDefaultTexture();	// Create default texture
};