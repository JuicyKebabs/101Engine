#pragma once

#include "Buffer.h"
#include "CommandContext.h"
#include "DescriptorHeap.h"
#include "Resource/TextureCpuData.h"

#include <memory>
#include <vector>

namespace rendering::backend
{

struct TextureGpuData
{
	Microsoft::WRL::ComPtr<ID3D12Resource> texture;

	[[nodiscard]] Result InitializeSrv(ID3D12Device* device);
	[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandle() const noexcept { return m_srv; }

private:
	// Temporary CPU descriptor storage. Replace with an allocation owned by the
	// Renderer View manager, based on Framework DescriptorHeapAllocator.
	// The descriptor and texture must both outlive their GPU/CPU users.
	DescriptorHeap m_srvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_srv{};
};

struct TextureUploadData
{
	Buffer buffer;
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts;
};

// Both outputs must be empty. Failure leaves them unchanged.
// cpu is the validated content produced by CopyTextureCpuData.
// No GPU commands are recorded or submitted by this function.
[[nodiscard]]
Result PrepareTextureUpload(
ID3D12Device* device,
const detail::TextureCpuData& cpu,
std::unique_ptr<TextureGpuData>& gpuOutput,
std::unique_ptr<TextureUploadData>& uploadOutput);

// One initial upload on the Graphics (DIRECT) queue only. 
// Caller retains the texture, upload and command allocator until submission completion. 
// On any recording failure, discard the entire list; never execute a partial upload.
[[nodiscard]]
Result RecordTextureUpload(
CommandContext& context,
const TextureGpuData& gpu,
const TextureUploadData& upload);

}

