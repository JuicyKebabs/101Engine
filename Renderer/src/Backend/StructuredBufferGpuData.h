#pragma once

#include "Buffer.h"
#include "DescriptorHeap.h"
#include "Renderer/StructuredBufferDesc.h"

//---------------------------------------------------------------------------------
// StructuredBufferGpuData
// Backend specification of the StructuredBufferGpuData class.
// This class manages the GPU resources for a structured buffer,
// including the buffer itself and the shader resource view (SRV) descriptor heap.
//---------------------------------------------------------------------------------

namespace rendering::backend
{

class Backend;

class StructuredBufferGpuData
{
public:
	Result Initialize(Backend& backend, const StructuredBufferDesc& desc);
	Result Write(Backend& backend, std::span<const std::byte> data);
	
	std::uint64_t GetSize() const { return m_buffer.GetSize(); }
	std::uint32_t GetElementStride() const noexcept { return m_elementStride; }
	
	Result CopyFrameSrv(Backend& backend, D3D12_GPU_DESCRIPTOR_HANDLE& output) const;

private:
	Buffer m_buffer;
	DescriptorHeap m_srvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle;
	std::uint32_t m_elementStride = 0;
};

}