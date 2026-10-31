#pragma once

#include "Backend/RenderTargetTextureGpuData.h"
#include "Renderer/DrawTypes.h"
#include "ResourceSlotTable.h"

#include <memory>
#include <vector>

namespace rendering::detail
{

// Revision is changed by Resize, not by copying the logical handle.
struct RenderTargetTextureContent
{
    // Owned value describing this physical revision's allocation policy.
    // Reconstructing only Format/extent from native views would lose the Mask's
    // optimized Clear on Resize. This contains no borrowed spans or GPU owners.
    RenderTargetTextureDesc creationDescription;
    std::uint64_t revision = 1;
    std::unique_ptr<backend::RenderTargetTextureGpuData> gpuData;
};

// Separate owner from CPU-upload textures. Handles remain stable across Resize.
class RenderTargetTextureRegistry
{
public:
    std::string DescribeUnavailable(RenderTargetTextureHandle handle) const
    {
        return DescribeResourceIdentity(handle, m_slots.ValidateForNewUse(handle));
    }

    explicit RenderTargetTextureRegistry(std::uint32_t ownerId) : m_slots(ownerId) {}

    ResourceRegistrationResult<RenderTargetTextureHandle> Create(ID3D12Device* device,
                                                                 const RenderTargetTextureDesc& description);

    // Caller establishes an inactive, confirmed-idle boundary before replacement.
    Result Resize(ID3D12Device* device, RenderTargetTextureHandle handle, std::uint32_t width, std::uint32_t height);

    Result Destroy(RenderTargetTextureHandle handle);
    Result QueryInfo(RenderTargetTextureHandle handle, RenderTargetTextureInfo& output) const;
    RenderTargetTextureContent* FindForNewUse(RenderTargetTextureHandle handle) noexcept;
    const RenderTargetTextureContent* FindForNewUse(RenderTargetTextureHandle handle) const noexcept;

private:
    ResourceSlotTable<RenderTargetTextureHandle> m_slots;
    std::vector<std::unique_ptr<RenderTargetTextureContent>> m_contents;
};

} // namespace rendering::detail
