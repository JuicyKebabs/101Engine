#include <filesystem>
#include <cstring>
#include "TextureManager.h"

using namespace DirectX;

// Get file extension from path
std::wstring FileExtension(const std::wstring& path)
{
	std::filesystem::path fsPath(path);				// Create filesystem path from file path
	return fsPath.extension().wstring().substr(1); // Remove leading dot from extension and return
}

// Initialization
void TextureManager::Initialize(
	ID3D12Device* pDevice,	// Device
	uint32_t maxDescriptors	// Maximum descriptor count
)
{
	m_pDevice = pDevice;	// Save device

	// SRV descriptor heap setup
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};	// Descriptor heap descriptor
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;		// SRV heap
	desc.NumDescriptors = maxDescriptors;					// Descriptor count
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // Shader visible

	// Create descriptor heap
	pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pSrvHeap));

	// Get SRV descriptor increment size
	m_srvIncrementSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	m_nextFreeIndex = static_cast<uint32_t>(TEXTURE_SRV_INDEX_RESERVED::RESERVED_COUNT);	// Initialize next free index
	m_loadedTextures.clear();																// Clear loaded texture map

	// Load default white texture
	CreateDefaultTexture();
}

// Load SRV from file
uint32_t TextureManager::LoadSrvFromFile(const std::wstring& path)
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
		return static_cast<uint32_t>(TEXTURE_SRV_INDEX_RESERVED::DEFAULT_TEXTURE);
	}

	auto ext = FileExtension(path);	// Get file extension

	HRESULT hr = S_FALSE;	// HRESULT

	// Switch loading method based on extension
	if (ext == L"tga")
	{// Load from TGA file
		hr = LoadFromTGAFile(path.c_str(), &meta, img);
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
		return static_cast<uint32_t>(TEXTURE_SRV_INDEX_RESERVED::DEFAULT_TEXTURE);
	}

	// Get image data
	size_t imageCount = img.GetImageCount();	// Get image count

	// If no image data, output error message and return
	if (imageCount == 0)
	{
		OutputDebugStringW((L"[TextureManager] No image data: " + path + L"\n").c_str());
		return static_cast<uint32_t>(TEXTURE_SRV_INDEX_RESERVED::DEFAULT_TEXTURE);
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

	// Allocate SRV descriptor index
	const uint32_t textureIndex = AllocateSrv();

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

	auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(	// CPU descriptor handle
		m_pSrvHeap->GetCPUDescriptorHandleForHeapStart(),	// Heap start handle
		textureIndex,										// Offset (number of loaded textures)
		m_srvIncrementSize									// Increment size
	);

	m_pDevice->CreateShaderResourceView(	// Create SRV
		pTexture.Get(),	// Texture resource
		&srvDesc,		// SRV descriptor
		cpuHandle		// SRV handle
	);

	// Register texture information
	m_loadedTextures[path] = textureIndex;			// Register path and index

	// Keep resources
	m_textures.push_back(pTexture);				// Keep texture resource

	// Get image data from ScratchImage
	const Image* images = img.GetImages();		// Get image data
	size_t count = img.GetImageCount();			// Get image count

	if (images == nullptr || count == 0)
	{
		OutputDebugStringA("[TextureManager] No image data in ScratchImage\n");
		return static_cast<uint32_t>(TEXTURE_SRV_INDEX_RESERVED::DEFAULT_TEXTURE);
	}

	// Register as pending texture upload
	PendingTextureUpload pending = {};	// Pending texture upload
	pending.texture = pTexture;						// Texture resource
	pending.uploadBuffer = pUploadBuffer;			// Upload buffer
	pending.srvIndex = textureIndex;				// SRV descriptor index

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

	return textureIndex;	// Return texture index
}

// Allocate SRV descriptor index (for manually created textures)
uint32_t TextureManager::AllocateSrv()
{
	if(m_nextFreeIndex >= m_pSrvHeap->GetDesc().NumDescriptors)
	{
		OutputDebugStringA("[TextureManager] No more SRV descriptors available\n");
		return UINT32_MAX; // or some fallback texture index
	}

	return m_nextFreeIndex++;
}

// Create SRV for a texture resource
void TextureManager::CreateSrv(
	ID3D12Resource* pResource,						// Texture resource
	DXGI_FORMAT format,								// Texture format
	uint32_t srvIndex								// SRV descriptor index
)
{
	if(!pResource) 	
	{
		OutputDebugStringA("[TextureManager] Invalid resource for CreateSrv\n");
		return;
	}

	if(srvIndex >= m_pSrvHeap->GetDesc().NumDescriptors)
	{
		OutputDebugStringA("[TextureManager] SRV index out of bounds for CreateSrv\n");
		return;
	}

	auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(	// CPU descriptor handle
		m_pSrvHeap->GetCPUDescriptorHandleForHeapStart(),	// Heap start handle
		srvIndex,											// Offset (SRV index)
		m_srvIncrementSize									// Increment size
	);

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
		if (!pending.texture || !pending.uploadBuffer || pending.subresources.empty())
		{
			OutputDebugStringA("PendingTextureUpload has null member\n");
			continue;
		}

		// Upload data to upload buffer
		UpdateSubresources(
			cmdList,										// Command list
			pending.texture.Get(),							// Destination resource
			pending.uploadBuffer.Get(),						// Source resource
			0,												// Source offset
			0,												// First subresource
			static_cast<UINT>(pending.subresources.size()),	// Number of subresources
			pending.subresources.data()						// Subresource array
		);

		// Change texture state from copy destination to pixel shader resource
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			pending.texture.Get(),						// Resource
			D3D12_RESOURCE_STATE_COPY_DEST,				// Before state
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE	// After state
		);
		cmdList->ResourceBarrier(1, &barrier);	// Set resource barrier

		m_uploadKeepAlive.push_back(pending.uploadBuffer); // Keep upload buffer alive until GPU is done
	}

	// Clear pending texture upload array
	m_pendingUploads.clear();
}

// Get SRV heap
ID3D12DescriptorHeap* TextureManager::GetSrvHeap() const
{
	return m_pSrvHeap.Get();
}

// Get SRV descriptor increment size
UINT TextureManager::GetSrvIncrementSize() const
{
	return m_srvIncrementSize;
}

// Get post-processing texture index
uint32_t TextureManager::GetPostProcessingTextureIndex() const
{
	return static_cast<uint32_t>(TEXTURE_SRV_INDEX_RESERVED::POST_PROCESSING);
}

// Get default white texture index
uint32_t TextureManager::GetDefaultTextureIndex() const
{
	return static_cast<uint32_t>(TEXTURE_SRV_INDEX_RESERVED::DEFAULT_TEXTURE);
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
	const uint32_t defaultTextureIndex = static_cast<uint32_t>(TEXTURE_SRV_INDEX_RESERVED::DEFAULT_TEXTURE); // Default white texture index
	CreateSrv(
		pTexture.Get(),					// Texture resource
		DXGI_FORMAT_R8G8B8A8_UNORM,		// Format
		defaultTextureIndex				// SRV descriptor index
	);

	m_textures.push_back(pTexture); // Keep texture resource alive

	// Register as pending texture upload
	PendingTextureUpload pending = {};	// Pending texture upload
	pending.texture = pTexture;						// Texture resource
	pending.uploadBuffer = pUploadBuffer;			// Upload buffer
	pending.srvIndex = defaultTextureIndex;			// SRV descriptor index

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
