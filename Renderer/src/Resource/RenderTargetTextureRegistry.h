// R7 implementation guide: section 4.9.
// Separate long-lived owner for GPU-generated output images.
// Generation describes logical handle reuse; revision describes replacement of the same image.
// Retired revisions survive until their submitted GPU use is known to have completed.

#pragma once

#include "Backend/RenderTargetTextureGpuData.h"
#include "Renderer/DrawTypes.h"
#include "ResourceSlotTable.h"

#include <memory>
#include <vector>

namespace rendering::detail
{

// Revision is changed by Resize, not by copying the logical handle.
// lastDrawFenceValue includes attachment-only writes such as Clear, not only Material reads.
struct RenderTargetTextureContent
{
	std::uint64_t revision = 1;
	std::uint64_t lastDrawFenceValue = 0;
	std::unique_ptr<backend::RenderTargetTextureGpuData> gpuData;
};

// Separate owner from CPU-upload textures. Handles remain stable across Resize.
class RenderTargetTextureRegistry
{
public:
	explicit RenderTargetTextureRegistry(std::uint32_t ownerId) : m_slots(ownerId) {}

	ResourceRegistrationResult<RenderTargetTextureHandle> Create(
		ID3D12Device* device,
		const RenderTargetTextureDesc& description);
	// Caller establishes an inactive, confirmed-idle boundary before replacement.
	Result Resize(ID3D12Device* device, RenderTargetTextureHandle handle,
		std::uint32_t width, std::uint32_t height);
	Result Destroy(RenderTargetTextureHandle handle);
	Result QueryInfo(RenderTargetTextureHandle handle, RenderTargetTextureInfo& output) const;
	RenderTargetTextureContent* FindForNewUse(RenderTargetTextureHandle handle) noexcept;
	const RenderTargetTextureContent* FindForNewUse(RenderTargetTextureHandle handle) const noexcept;
	void Collect(std::uint64_t completedFence);

private:
	ResourceSlotTable<RenderTargetTextureHandle> m_slots;
	std::vector<std::unique_ptr<RenderTargetTextureContent>> m_contents;
	// Old physical revisions wait here after replacement.
	// The current slot can publish a new revision while old submitted GPU use remains protected.
	std::vector<std::unique_ptr<RenderTargetTextureContent>> m_retired;
};

}
