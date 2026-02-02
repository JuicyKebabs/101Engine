#include "TextureManager.h"
#include "d3dx12.h"
#include "DirectXTex.h"
#include <filesystem>

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

	m_nextFreeIndex = 0;		// Initialize next free index
	m_loadedTextures.clear();	// Clear loaded texture map

	// Load default white texture
	m_defaultTextureIndex = LoadSrvFromFile(L"asset/texture/white.png");
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
		return UINT32_MAX; // or some fallback texture index
	}

	// Get image data
	size_t imageCount = img.GetImageCount();	// Get image count

	// If no image data, output error message and return
	if (imageCount == 0)
	{
		OutputDebugStringW((L"[TextureManager] No image data: " + path + L"\n").c_str());
		return UINT32_MAX;
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

	// Set subresource data
	const uint32_t textureIndex = m_nextFreeIndex;		// Get next texture index
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;	// Subresource array
	subresources.reserve(img.GetImageCount());			// Reserve array size
	for (size_t i = 0; i < img.GetImageCount(); ++i)
	{
		const Image* imgData = img.GetImages() + i; // Get image data

		D3D12_SUBRESOURCE_DATA subresource = {};	// Subresource data
		subresource.pData = imgData->pixels;			// Pixel data
		subresource.RowPitch = imgData->rowPitch;		// Row pitch
		subresource.SlicePitch = imgData->slicePitch;	// Slice pitch
		subresources.push_back(subresource);			// Add to array
	}

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
	m_nextFreeIndex++;								// Update next free index

	// Keep resources and upload buffer alive
	m_textures.push_back(pTexture);				// Keep texture resource
	m_uploadKeepAlive.push_back(pUploadBuffer); // Keep upload buffer

	// Copy to keep ScratchImage alive

	auto scratch = std::make_unique<ScratchImage>(std::move(img));

	// Register as pending texture upload
	PendingTextureUpload pending = {};	// Pending texture upload
	pending.texture = pTexture;						// Texture resource
	pending.uploadBuffer = pUploadBuffer;			// Upload buffer
	pending.image = std::move(scratch);				// Scratch image
	pending.srvIndex = textureIndex;				// SRV descriptor index
	m_pendingUploads.push_back(std::move(pending)); // Add to array

	return textureIndex;	// Return texture index
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
		if (!pending.texture || !pending.uploadBuffer || !pending.image)
		{
			OutputDebugStringA("PendingTextureUpload has null member\n");
			continue;
		}

		// Get image data from ScratchImage
		const ScratchImage& img = *pending.image;	// ScratchImage
		const Image* images = img.GetImages();		// Get image data
		size_t count = img.GetImageCount();			// Get image count

		if(images == nullptr || count == 0)
		{
			continue; // Skip invalid image data
		}

		// Create temporary array here
		std::vector<D3D12_SUBRESOURCE_DATA> subresources(count);

		for (size_t i = 0; i < count; ++i)
		{
			D3D12_SUBRESOURCE_DATA s{};
			s.pData = images[i].pixels;
			s.RowPitch = images[i].rowPitch;
			s.SlicePitch = images[i].slicePitch;
			subresources[i] = s;
		}

		// Upload data to upload buffer
		UpdateSubresources(
			cmdList,					// Command list
			pending.texture.Get(),		// Destination resource
			pending.uploadBuffer.Get(),	// Source resource
			0,							// Source offset
			0,							// First subresource
			static_cast<UINT>(count),	// Number of subresources
			subresources.data()			// Subresource array
		);

		// Change texture state from copy destination to pixel shader resource
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			pending.texture.Get(),						// Resource
			D3D12_RESOURCE_STATE_COPY_DEST,				// Before state
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE	// After state
		);
		cmdList->ResourceBarrier(1, &barrier);	// Set resource barrier
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

// Get default white texture index
uint32_t TextureManager::GetDefaultWhiteTextureIndex() const
{
	return m_defaultTextureIndex;
}
