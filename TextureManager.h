#pragma once
#include <d3d12.h>
#include "DirectXTex.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include "ComPtr.h"

// Pending texture upload structure
struct PendingTextureUpload
{
	ComPtr<ID3D12Resource> texture;						// Texture resource
	ComPtr<ID3D12Resource> uploadBuffer;				// Upload buffer
	std::unique_ptr<DirectX::ScratchImage> image;		// Scratch image
	uint32_t srvIndex;									// SRV descriptor index
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

	void UploadPendingTextures(ID3D12GraphicsCommandList* cmdList);	// Upload pending textures

	ID3D12DescriptorHeap* GetSrvHeap() const;	// Get SRV heap (where SRVs are stored)
	UINT GetSrvIncrementSize() const;			// Get SRV descriptor increment size

	uint32_t GetDefaultWhiteTextureIndex() const;	// Get default white texture index

private:
	ComPtr<ID3D12DescriptorHeap> m_pSrvHeap;	// SRV descriptor heap (where texture SRVs are stored)

	UINT m_srvIncrementSize = 0;	// SRV descriptor increment size
	UINT m_nextFreeIndex = 0;		// Next free descriptor index

	std::unordered_map<std::wstring, uint32_t> m_loadedTextures;	// Map of loaded textures
	std::vector<ComPtr<ID3D12Resource>> m_textures;					// Array of texture resources
	std::vector< ComPtr<ID3D12Resource> > m_uploadKeepAlive;		// Keep-alive array for upload buffers
	std::vector<PendingTextureUpload> m_pendingUploads;				// Array of pending texture uploads

	ID3D12Device* m_pDevice = nullptr;	// Device

	uint32_t m_defaultTextureIndex = UINT32_MAX; // Default white texture index
};