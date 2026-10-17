#pragma once

#include "Renderer/Result.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <cstddef>
#include <cstdint>
#include <span>

//-----------------------------------------------------
// Buffer
// A wrapper for a GPU buffer resource in Direct3D 12.
//-----------------------------------------------------

namespace rendering::backend
{

enum class BufferMemory
{
	Default,
	Upload,
};

class Buffer
{
public:
	Buffer() = default;

	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;
	Buffer(Buffer&&) = delete;
	Buffer& operator=(Buffer&&) = delete;

	Result Initialize(ID3D12Device* device, std::uint64_t size, BufferMemory memory);

	// Caller must ensure the GPU no longer uses this resource.
	void Finalize() noexcept;

	// Writes from offset zero.
	// Caller must ensure no conflicting GPU use.
	Result Write(std::span<const std::byte> data);

	ID3D12Resource* GetNativeResource() const noexcept
	{
		return m_resource.Get();
	}

	std::uint64_t GetSize() const noexcept
	{
		return m_size;
	}

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
	std::uint64_t m_size = 0;
	BufferMemory m_memory = BufferMemory::Default;
};

}