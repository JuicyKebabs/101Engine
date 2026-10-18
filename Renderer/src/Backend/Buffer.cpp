#include "Buffer.h"
#include "ApiResult.h"

#include <cstring>
#include <utility>

namespace rendering::backend
{

Result Buffer::Initialize(ID3D12Device* device, std::uint64_t size, BufferMemory memory)
{
	if (m_resource)
	{
		return { false, "Buffer is already initialized." };
	}

	if (!device || size == 0)
	{
		return { false, "Buffer requires a device and a positive size." };
	}

	if (memory != BufferMemory::Default && memory != BufferMemory::Upload)
	{
		return { false, "Unsupported buffer memory type." };
	}

	const bool upload = memory == BufferMemory::Upload;

	D3D12_HEAP_PROPERTIES props{};
	props.Type = upload ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
	props.CreationNodeMask = 1;
	props.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = size;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	const auto initialState = upload ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;

	// Create the resource for temporal buffer.
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;

	const HRESULT hr = device->CreateCommittedResource(
		&props,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		initialState,
		nullptr,
		IID_PPV_ARGS(&resource));

	if (FAILED(hr))
	{
		return MakeApiFailure("CreateCommittedResource(Buffer)", hr);
	}

	// Store resource and its properties in the object after successful creation.
	m_resource = std::move(resource);
	m_size = size;
	m_memory = memory;
	return {};
}

void Buffer::Finalize() noexcept
{
	m_resource.Reset();
	m_size = 0;
	m_memory = BufferMemory::Default;
}

Result Buffer::Write(std::span<const std::byte> data)
{
	return WriteAt(0, data);
}

Result Buffer::WriteAt(std::uint64_t offset, std::span<const std::byte> data)
{
	if (!m_resource || m_memory != BufferMemory::Upload)
	{
		return { false, "Write requires an initialized Upload buffer." };
	}

	// Validate the offset size and data size
	if (offset > m_size || data.size() > m_size - offset ||
		offset > (std::numeric_limits<SIZE_T>::max)() ||
		data.size() > (std::numeric_limits<SIZE_T>::max)() - static_cast<SIZE_T>(offset))
	{
		return { false, "Upload data exceeds buffer size." };
	}

	if (data.empty())
	{
		return {};
	}

	void* mapped = nullptr;
	const D3D12_RANGE readRange{ 0, 0 };

	// Map the resource to get a pointer to the memory where we can write the data
	const HRESULT hr = m_resource->Map(0, &readRange, &mapped);
	if (FAILED(hr))
	{
		return MakeApiFailure("ID3D12Resource::Map(Upload)", hr);
	}

	// Copy the data to the mapped memory at the specified offset
	std::memcpy(
		static_cast<std::byte*>(mapped) + static_cast<SIZE_T>(offset),
		data.data(),
		data.size());

	// Unmap the resource, specifying the range of data that was written
	const D3D12_RANGE writtenRange{
			static_cast<SIZE_T>(offset),
			static_cast<SIZE_T>(offset) + data.size(),
	};

	m_resource->Unmap(0, &writtenRange);

	return {};
			
}

}