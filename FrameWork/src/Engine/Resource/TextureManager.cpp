#include <filesystem>
#include <cstring>
#include "TextureManager.h"
#include "Engine/Core/Debug/Debug.h"

using namespace DirectX;

// Get file extension from path
std::wstring FileExtension(const std::wstring& path)
{
	std::filesystem::path fsPath(path);				// Create filesystem path from file path
	return fsPath.extension().wstring().substr(1); // Remove leading dot from extension and return
}

TextureManager* TextureManager::GetInstance()
{
	static TextureManager instance;
	return &instance;
}

// Initialization
void TextureManager::Initialize(ID3D12Device* pDevice, DescriptorHeapAllocator* pDescriptorHeapAllocator)
{
	m_pDevice = pDevice;									// Save device
	m_pDescriptorHeapAllocator = pDescriptorHeapAllocator;	// Save descriptor heap allocator

	CreateDefaultTexture();
	CreateErrorTexture();
}

// Load SRV from file
TextureHandle TextureManager::LoadTexture(const std::wstring& path)
{
	// If already loaded, return the index
	if (auto it = m_loadedTextures.find(path); it != m_loadedTextures.end())
	{
		return it->second;
	}

	// Load image
	ScratchImage img = {};	// Scratch image
	TexMetadata meta = {};		// Metadata

	if(path.empty())
	{
		OutputDebugStringW(L"[TextureManager] Empty path provided.\n");
		return m_errorTextureHandle;
	}

	auto ext = FileExtension(path);	// Get file extension

	HRESULT hr = S_FALSE;	// HRESULT

	// Switch loading method based on extension
	if (ext == L"tga")
	{// Load from TGA file
		hr = LoadFromTGAFile(path.c_str(), &meta, img);
	}
	else if (ext == L"dds")
	{// Load from DDS file
		hr = LoadFromDDSFile(path.c_str(), DDS_FLAGS_NONE, &meta, img);
	}
	else
	{// Load from WIC file
		hr = LoadFromWICFile(		// Load from WIC file
			path.c_str(),	// File path
			WIC_FLAGS_NONE,	// WIC flags
			&meta,			// Metadata
			img				// Scratch image
		);
	}

	// If loading failed, output error message and return
	if (FAILED(hr))
	{
		OutputDebugStringW((L"[TextureManager] Failed to load: " + path + L"\n").c_str());
		return m_errorTextureHandle;
	}

	// Get image data
	size_t imageCount = img.GetImageCount();	// Get image count

	// If no image data, output error message and return
	if (imageCount == 0)
	{
		OutputDebugStringW((L"[TextureManager] No image data: " + path + L"\n").c_str());
		return m_errorTextureHandle;
	}

	// Create texture resource
	ComPtr<ID3D12Resource> pTexture;	// Texture resource
	CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(	// Texture resource descriptor
		meta.format,						// Format
		static_cast<UINT>(meta.width),		// Width
		static_cast<UINT>(meta.height),		// Height
		1,									// Array size
		static_cast<UINT>(meta.mipLevels)	// Mip levels
	);

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT); // Heap properties (default)
	hr =  m_pDevice->CreateCommittedResource(	// Create resource
		&heapProps,						// Heap properties
		D3D12_HEAP_FLAG_NONE,			// Heap flags
		&texDesc,						// Resource descriptor
		D3D12_RESOURCE_STATE_COPY_DEST,	// Initial resource state
		nullptr,						// Optimized clear value
		IID_PPV_ARGS(&pTexture)			// Resource to create
	);

	// Create upload buffer
	ComPtr<ID3D12Resource> pUploadBuffer;	// Upload buffer
	const UINT numSubresources = static_cast<UINT>(imageCount); // Get subresource count
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(	// Get upload buffer size
		pTexture.Get(),	// Texture resource
		0,				// First subresource
		numSubresources	// Mip levels
	);

	auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD); // Heap properties (upload)
	auto uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize); // Buffer resource descriptor

	m_pDevice->CreateCommittedResource(	// Create resource
		&uploadHeapProps,					// Heap properties
		D3D12_HEAP_FLAG_NONE,				// Heap flags
		&uploadBufferDesc,					// Resource descriptor
		D3D12_RESOURCE_STATE_GENERIC_READ,	// Initial resource state
		nullptr,							// Optimized clear value
		IID_PPV_ARGS(&pUploadBuffer)		// Resource to create
	);

	// Create shader resource view
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};	// SRV descriptor
	srvDesc.Shader4ComponentMapping = 
		D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;	// Component mapping
	srvDesc.Format = 
		meta.format;								// Format
	srvDesc.ViewDimension = 
		D3D12_SRV_DIMENSION_TEXTURE2D;				// View dimension (2D texture)
	srvDesc.Texture2D.MipLevels = 
		(UINT)meta.mipLevels;						// Mip levels

	auto srvIndex = m_pDescriptorHeapAllocator->AllocateCbvSrvUav();				// Allocate SRV descriptor index
	auto cpuHandle = m_pDescriptorHeapAllocator->GetCbvSrvUavCpuHandle(srvIndex);	// Get CPU descriptor handle for SRV

	m_pDevice->CreateShaderResourceView(	// Create SRV
		pTexture.Get(),	// Texture resource
		&srvDesc,		// SRV descriptor
		cpuHandle		// SRV handle
	);

	// Create texture data and store in map
	std::unique_ptr<Texture> textureData = std::make_unique<Texture>();	// Texture data
	textureData->Initialize(
		pTexture,		// Texture resource
		Texture::ParamDesc{
		m_nextTextureHandle,	// Texture handle (using next free index)
		srvIndex,				// SRV index
		path,					// File path
		meta					// Metadata
		}
	);
	m_loadedTextures[path] = m_nextTextureHandle;	// Store in map (path to texture handle)
	m_nextTextureHandle++;							// Increment next free index
	textureData->MarkAsPending();					// Mark as pending upload

	// Keep resources
	TextureHandle textureHandle = textureData->GetHandle();	// Get texture handle for map storage
	m_textures[textureHandle] = std::move(textureData);		// Store texture data in map (texture handle to texture data)

	// Get image data from ScratchImage
	const Image* images = img.GetImages();		// Get image data
	size_t count = img.GetImageCount();			// Get image count

	if (images == nullptr || count == 0)
	{
		DBG("[TextureManager] No image data in ScratchImage\n");
		return m_errorTextureHandle;
	}

	// Register as pending texture upload
	PendingTextureUpload pending = {};	// Pending texture upload
	pending.textureHandle = textureHandle;	// Texture resource
	pending.uploadBuffer = pUploadBuffer;	// Upload buffer

	// Calculate total size of pixel and reserve space in owned data vector
	auto pixels = img.GetImages()->pixels;	// Get pixel data pointer
	size_t totalSize = 0;					// Total size of pixel data
	for (size_t i = 0; i < count; i++)
	{
		totalSize += img.GetImages()[i].slicePitch; // Accumulate slice pitch for each image
	}
	pending.ownedData.resize(totalSize);	// Resize owned data vector to fit pixel data

	// Prepare subresource data for texture upload
	pending.subresources.resize(count);	// Resize subresource array to match image count
	size_t offset = 0;					// Offset for copying pixel data
	for (size_t i = 0; i < count; ++i)
	{
		std::memcpy(pending.ownedData.data() + offset, images[i].pixels, img.GetImages()[i].slicePitch); // Copy pixel data to owned data vector

		D3D12_SUBRESOURCE_DATA subresource = {};	// Subresource data
		subresource.pData = pending.ownedData.data() + offset;	// Pixel data
		subresource.RowPitch = images[i].rowPitch;				// Row pitch
		subresource.SlicePitch = images[i].slicePitch;			// Slice pitch

		pending.subresources[i] = subresource;		// Store subresource data
		offset += img.GetImages()[i].slicePitch;	// Move offset for next image
	}

	m_pendingUploads.push_back(std::move(pending)); // Add to array

	return textureHandle;	// Return texture index
}

// Create SRV for a texture resource
SrvIndex TextureManager::CreateSrv(
	ID3D12Resource* pResource,	// Texture resource
	DXGI_FORMAT format			// Texture format
)
{
	auto srvIndex = m_pDescriptorHeapAllocator->AllocateCbvSrvUav();				// Allocate SRV descriptor index
	auto cpuHandle = m_pDescriptorHeapAllocator->GetCbvSrvUavCpuHandle(srvIndex);	// Get CPU descriptor handle for SRV

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};	// SRV descriptor
	srvDesc.Shader4ComponentMapping = 
		D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;	// Component mapping
	srvDesc.Format = format;						// Format
	srvDesc.ViewDimension = 
		D3D12_SRV_DIMENSION_TEXTURE2D;				// View dimension (2D texture)
	srvDesc.Texture2D.MipLevels = 1;				// Mip levels (1 for manually created textures)
	srvDesc.Texture2D.MostDetailedMip = 0;			// Most detailed mip (0 for manually created textures)
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;	// Resource min LOD clamp

	m_pDevice->CreateShaderResourceView(	// Create SRV
		pResource,		// Texture resource
		&srvDesc,		// SRV descriptor
		cpuHandle		// SRV handle
	);

	return srvIndex;	// Return SRV index
}

// Upload pending textures
void TextureManager::UploadPendingTextures(ID3D12GraphicsCommandList* cmdList)
{
	// If command list is invalid or no pending texture uploads, do nothing
	if (!cmdList || m_pendingUploads.empty())
	{
		return;
	}

	// Upload each pending texture
	for(auto& pending : m_pendingUploads)
	{
		auto texture = GetTexture(pending.textureHandle); // Get texture resource

		if (!texture || !pending.uploadBuffer || pending.subresources.empty())
		{
			DBG("PendingTextureUpload has null member\n");
			if (texture) texture->MarkAsFailed(); // Mark texture as failed if it exists
			continue;
		}

		// Upload data to upload buffer
		UpdateSubresources(
			cmdList,										// Command list
			texture->GetResource(),							// Destination resource
			pending.uploadBuffer.Get(),						// Source resource
			0,												// Source offset
			0,												// First subresource
			static_cast<UINT>(pending.subresources.size()),	// Number of subresources
			pending.subresources.data()						// Subresource array
		);

		// Change texture state from copy destination to pixel shader resource
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			texture->GetResource(),						// Resource
			D3D12_RESOURCE_STATE_COPY_DEST,				// Before state
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE	// After state
		);
		cmdList->ResourceBarrier(1, &barrier);	// Set resource barrier

		m_uploadKeepAlive.push_back(pending.uploadBuffer); // Keep upload buffer alive until GPU is done

		texture->MarkAsReady(); // Mark texture as uploaded
	}

	// Clear pending texture upload array
	m_pendingUploads.clear();
}

// Get texture by handle
Texture* TextureManager::GetTexture(TextureHandle handle) const
{
	auto it = m_textures.find(handle);
	if (it != m_textures.end()) {
		return it->second.get();
	}
	return nullptr;
}

// Get SRV index for a specific texture
SrvIndex TextureManager::GetTextureSrvIndex(TextureHandle handle) const
{
	if(handle == static_cast<TextureHandle>(InvalidTextureHandle)){
		DBG("[TextureManager] Invalid texture handle for GetTextureSrvIndex\n");
		return m_defaultTextureHandle; // Return default texture index for invalid handle
	}

	auto texture = GetTexture(handle);
	if (texture) {
		if(texture->GetState() != Texture::State::Ready){
			DBG("[TextureManager] Texture handle %u is not ready (state: %u)\n", handle, static_cast<uint32_t>(texture->GetState()));
		}
		else {
			SrvIndex srvIndex = texture->GetSrvIndex();
			if (srvIndex != InvalidSrvIndex)
			{
				return srvIndex;
			}
			else
			{
				DBG("[TextureManager] Texture handle %u has invalid SRV index\n", handle);
			}
		}
	} else{
		DBG("[TextureManager] Texture handle not found for GetTextureSrvIndex: %u\n", handle);
	}
	return m_defaultTextureHandle; // Return default texture index if not found
}

// Create default texture
void TextureManager::CreateDefaultTexture()
{
	// Create a 1x1 white texture	
	// Create texture resource
	ComPtr<ID3D12Resource> pTexture;	// Texture resource
	CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(	// Texture resource descriptor
		DXGI_FORMAT_R8G8B8A8_UNORM,			// Format (RGBA8)
		1,									// Width
		1,									// Height
		1,									// Array size
		1									// Mip levels
	);

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT); // Heap properties (default)
	m_pDevice->CreateCommittedResource(	// Create resource
		&heapProps,						// Heap properties
		D3D12_HEAP_FLAG_NONE,			// Heap flags
		&texDesc,						// Resource descriptor
		D3D12_RESOURCE_STATE_COPY_DEST, // Initial resource state
		nullptr,						// Optimized clear value
		IID_PPV_ARGS(&pTexture)			// Resource to create
	);

	// Create upload buffer for default texture
	std::vector<uint32_t> defaultPixel = { 0xFFFFFFFF }; // White pixel data (RGBA)
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize( // Get upload buffer size
		pTexture.Get(),				 // Texture resource
		0,							 // First subresource
		1							 // Mip levels
	);
	auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD); // Heap properties (upload)
	auto uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize); // Buffer resource descriptor
	ComPtr<ID3D12Resource> pUploadBuffer;	// Upload buffer
	m_pDevice->CreateCommittedResource(	// Create resource
		&uploadHeapProps,					// Heap properties
		D3D12_HEAP_FLAG_NONE,				// Heap flags
		&uploadBufferDesc,					// Resource descriptor
		D3D12_RESOURCE_STATE_GENERIC_READ,	// Initial resource state
		nullptr,							// Optimized clear value
		IID_PPV_ARGS(&pUploadBuffer)		// Resource to create
	);

	// Create shader resource view
	auto srvIndex = CreateSrv(
		pTexture.Get(),					// Texture resource
		DXGI_FORMAT_R8G8B8A8_UNORM		// Format
	);

	const uint32_t defaultTextureHandle = m_nextTextureHandle;	// Default texture handle (using next free index)
	auto textureData = std::make_unique<Texture>();	// Texture data
	Texture::ParamDesc paramDesc = {};		// Texture initialization descriptor
	paramDesc.handle = defaultTextureHandle;	// Texture handle (using reserved index)
	paramDesc.srvIndex = srvIndex;			// SRV index (using next free index)
	paramDesc.path = L"DefaultWhite";		// File path (for identification)
	paramDesc.meta = TexMetadata{			// Metadata
		.width = 1,								// Width
		.height = 1,							// Height
		.mipLevels = 1,							// Mip levels
		.format = DXGI_FORMAT_R8G8B8A8_UNORM,	// Format
		.dimension = TEX_DIMENSION_TEXTURE2D	// Dimension
	};
	textureData->Initialize(
		pTexture,
		paramDesc
	);

	m_textures[defaultTextureHandle] = std::move(textureData);	// Keep texture resource alive
	m_defaultTextureHandle = defaultTextureHandle;				// Store default texture handle
	m_nextTextureHandle++;										// Increment next free index

	// Register as pending texture upload
	PendingTextureUpload pending = {};	// Pending texture upload
	pending.textureHandle = defaultTextureHandle;	// Texture resource
	pending.uploadBuffer = pUploadBuffer;			// Upload buffer

	// Prepare owned data for upload
	pending.ownedData.resize(sizeof(uint32_t));	// Resize owned data to hold pixel data
	memcpy(pending.ownedData.data(), defaultPixel.data(), sizeof(uint32_t)); // Copy pixel data to owned data

	// Set subresource data for upload
	D3D12_SUBRESOURCE_DATA subresource = {};		// Subresource data
	subresource.pData = pending.ownedData.data();	// Pixel data
	subresource.RowPitch = sizeof(uint32_t);		// Row pitch
	subresource.SlicePitch = sizeof(uint32_t);		// Slice pitch
	pending.subresources.push_back(subresource);	// Subresource data

	m_pendingUploads.push_back(std::move(pending)); // Add to array
}

void TextureManager::CreateErrorTexture()
{
	// Create a 1x1 pink texture	
// Create texture resource
	ComPtr<ID3D12Resource> pTexture;	// Texture resource
	CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(	// Texture resource descriptor
		DXGI_FORMAT_R8G8B8A8_UNORM,			// Format (RGBA8)
		1,									// Width
		1,									// Height
		1,									// Array size
		1									// Mip levels
	);

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT); // Heap properties (default)
	m_pDevice->CreateCommittedResource(	// Create resource
		&heapProps,						// Heap properties
		D3D12_HEAP_FLAG_NONE,			// Heap flags
		&texDesc,						// Resource descriptor
		D3D12_RESOURCE_STATE_COPY_DEST, // Initial resource state
		nullptr,						// Optimized clear value
		IID_PPV_ARGS(&pTexture)			// Resource to create
	);

	// Create upload buffer for default texture
	std::vector<uint32_t> defaultPixel = { 0xFFFF00FF }; // Pink pixel data (RGBA)
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize( // Get upload buffer size
		pTexture.Get(),				 // Texture resource
		0,							 // First subresource
		1							 // Mip levels
	);
	auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD); // Heap properties (upload)
	auto uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize); // Buffer resource descriptor
	ComPtr<ID3D12Resource> pUploadBuffer;	// Upload buffer
	m_pDevice->CreateCommittedResource(	// Create resource
		&uploadHeapProps,					// Heap properties
		D3D12_HEAP_FLAG_NONE,				// Heap flags
		&uploadBufferDesc,					// Resource descriptor
		D3D12_RESOURCE_STATE_GENERIC_READ,	// Initial resource state
		nullptr,							// Optimized clear value
		IID_PPV_ARGS(&pUploadBuffer)		// Resource to create
	);

	// Create shader resource view
	auto srvIndex = CreateSrv(
		pTexture.Get(),					// Texture resource
		DXGI_FORMAT_R8G8B8A8_UNORM		// Format
	);

	const uint32_t defaultTextureHandle = m_nextTextureHandle;	// Default texture handle (using next free index)
	auto textureData = std::make_unique<Texture>();	// Texture data
	Texture::ParamDesc paramDesc = {};		// Texture initialization descriptor
	paramDesc.handle = defaultTextureHandle;	// Texture handle (using reserved index)
	paramDesc.srvIndex = srvIndex;			// SRV index (using next free index)
	paramDesc.path = L"DefaultWhite";		// File path (for identification)
	paramDesc.meta = TexMetadata{			// Metadata
		.width = 1,								// Width
		.height = 1,							// Height
		.mipLevels = 1,							// Mip levels
		.format = DXGI_FORMAT_R8G8B8A8_UNORM,	// Format
		.dimension = TEX_DIMENSION_TEXTURE2D	// Dimension
	};
	textureData->Initialize(
		pTexture,
		paramDesc
	);

	m_textures[defaultTextureHandle] = std::move(textureData);	// Keep texture resource alive
	m_defaultTextureHandle = defaultTextureHandle;				// Store default texture handle
	m_nextTextureHandle++;										// Increment next free index

	// Register as pending texture upload
	PendingTextureUpload pending = {};	// Pending texture upload
	pending.textureHandle = defaultTextureHandle;	// Texture resource
	pending.uploadBuffer = pUploadBuffer;			// Upload buffer

	// Prepare owned data for upload
	pending.ownedData.resize(sizeof(uint32_t));	// Resize owned data to hold pixel data
	memcpy(pending.ownedData.data(), defaultPixel.data(), sizeof(uint32_t)); // Copy pixel data to owned data

	// Set subresource data for upload
	D3D12_SUBRESOURCE_DATA subresource = {};		// Subresource data
	subresource.pData = pending.ownedData.data();	// Pixel data
	subresource.RowPitch = sizeof(uint32_t);		// Row pitch
	subresource.SlicePitch = sizeof(uint32_t);		// Slice pitch
	pending.subresources.push_back(subresource);	// Subresource data

	m_pendingUploads.push_back(std::move(pending)); // Add to array
}