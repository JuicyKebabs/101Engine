// R7 implementation guide: section 4.10.
// Registration publishes a complete GPU set; Resize swaps complete revisions transactionally.
// Destroy first stops new use, then Collect releases physical storage after the use Fence.
// All returned Content pointers are short borrows; Render prevents mutation during those borrows.

#include "RenderTargetTextureRegistry.h"
#include "Failure.h"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>

namespace rendering::detail
{

namespace
{

Result ClassifyDeviceFailure(ID3D12Device* device, Result failure)
{
	if (device && FAILED(device->GetDeviceRemovedReason()))
	{
		failure.failureKind = FailureKind::DeviceLost;
	}
	return failure;
}

}

// Prepare the complete GPU allocation before registering a usable slot.
// CPU allocation failures release only private candidates; no incomplete handle is returned.
ResourceRegistrationResult<RenderTargetTextureHandle> RenderTargetTextureRegistry::Create(
	ID3D12Device* device,
	const RenderTargetTextureDesc& description)
{
	ResourceRegistrationResult<RenderTargetTextureHandle> registration;
	try
	{
		auto candidate = std::make_unique<RenderTargetTextureContent>();
		candidate->gpuData = std::make_unique<backend::RenderTargetTextureGpuData>();
		registration.result = candidate->gpuData->Initialize(device, description);
		if (!registration.result.IsSuccess())
		{
			registration.result = ClassifyDeviceFailure(device, std::move(registration.result));
			return registration;
		}

		const auto slotIndex = m_slots.FindRegistrationSlotIndex();
		const bool slotSupported = slotIndex < RenderTargetTextureHandle::InvalidIndex &&
			slotIndex < m_contents.max_size();
		if (!slotSupported)
		{
			registration.result = Fail("RenderTargetTexture: slot limit reached.");
			return registration;
		}

		if (slotIndex >= m_contents.size())
		{
			m_contents.resize(slotIndex + 1);
		}

		auto slot = m_slots.Register();
		registration.result = std::move(slot.status);
		if (!registration.result.IsSuccess())
		{
			return registration;
		}

		m_contents[slot.handle.slotIndex] = std::move(candidate);
		registration.handle = slot.handle;
	}
	catch (const std::bad_alloc&)
	{
		registration.result = Fail("RenderTargetTexture: allocation failed.");
	}
	catch (const std::length_error&)
	{
		registration.result = Fail("RenderTargetTexture: storage size is unsupported.");
	}
	return registration;
}

// Precondition: the public Renderer already established an inactive, confirmed-idle boundary.
// Build a new complete revision first. The old dimensions, revision and GPU set survive any preparation failure.
// Same-size requests are no-ops so valid existing contents are not unnecessarily discarded.
Result RenderTargetTextureRegistry::Resize(
	ID3D12Device* device,
	RenderTargetTextureHandle handle,
	std::uint32_t width,
	std::uint32_t height)
{
	auto* current = FindForNewUse(handle);
	if (!current)
	{
		return Fail("ResizeRenderTargetTexture: handle is unavailable.");
	}

	if (width == 0 || height == 0)
	{
		return Fail("ResizeRenderTargetTexture: dimensions must be non-zero.");
	}

	if (current->gpuData->width == width && current->gpuData->height == height)
	{
		return {};
	}

	if (current->revision == (std::numeric_limits<std::uint64_t>::max)())
	{
		return Fail("ResizeRenderTargetTexture: revision is exhausted.");
	}

	try
	{
		RenderTargetTextureDesc description;
		description.width = width;
		description.height = height;

		auto candidate = std::make_unique<RenderTargetTextureContent>();
		candidate->revision = current->revision + 1;
		candidate->gpuData = std::make_unique<backend::RenderTargetTextureGpuData>();
		Result prepared = candidate->gpuData->Initialize(device, description);
		if (!prepared.IsSuccess())
		{
			return ClassifyDeviceFailure(device, std::move(prepared));
		}

		// This allocation is the final potentially throwing operation before ownership exchange.
		// Moving the current owner first would lose the old published output if vector growth failed.
		// Allocate retirement storage before moving the live owner.
		m_retired.emplace_back();
		m_retired.back() = std::move(m_contents[handle.slotIndex]);
		m_contents[handle.slotIndex] = std::move(candidate);
		return {};
	}
	catch (const std::bad_alloc&)
	{
		return Fail("ResizeRenderTargetTexture: allocation failed; previous output retained.");
	}
	catch (const std::length_error&)
	{
		return Fail("ResizeRenderTargetTexture: storage size unsupported; previous output retained.");
	}
}

// Logical invalidation is immediate for all new View and Material resolution.
// Physical deletion is deferred to Collect; callers must not confuse these two times.
Result RenderTargetTextureRegistry::Destroy(RenderTargetTextureHandle handle)
{
	if (m_slots.RequestDestroy(handle) != ResourceHandleValidationResult::Success)
	{
		return Fail("DestroyRenderTargetTexture: handle is unavailable.");
	}
	return {};
}

// Validate owner/generation/destroy state before returning a short borrow.
// Render's no-reentry rule prevents this owner from being replaced during Graph declaration and recording.
RenderTargetTextureContent* RenderTargetTextureRegistry::FindForNewUse(RenderTargetTextureHandle handle) noexcept
{
	if (m_slots.ValidateForNewUse(handle) != ResourceHandleValidationResult::Success)
	{
		return nullptr;
	}
	return m_contents[handle.slotIndex].get();
}

const RenderTargetTextureContent* RenderTargetTextureRegistry::FindForNewUse(RenderTargetTextureHandle handle) const noexcept
{
	if (m_slots.ValidateForNewUse(handle) != ResourceHandleValidationResult::Success)
	{
		return nullptr;
	}
	return m_contents[handle.slotIndex].get();
}

// Fill a local value and assign the caller's storage only after validation succeeds.
// A failed query leaves the caller's previous Info unchanged.
Result RenderTargetTextureRegistry::QueryInfo(RenderTargetTextureHandle handle, RenderTargetTextureInfo& output) const
{
	const auto* current = FindForNewUse(handle);
	if (!current || !current->gpuData)
	{
		return Fail("GetRenderTargetTextureInfo: handle is unavailable.");
	}

	RenderTargetTextureInfo information;
	information.width = current->gpuData->width;
	information.height = current->gpuData->height;
	information.revision = current->revision;
	information.contentsValid = current->gpuData->contentsValid;
	output = information;
	return {};
}

// Only completed Queue use allows physical release.
// Retired revisions and Destroy-requested current slots are two different sources of releasable storage.
// Release a slot after destroying its resource so the next registration receives a new generation.
void RenderTargetTextureRegistry::Collect(std::uint64_t completedFence)
{
	std::erase_if(m_retired, [completedFence](const auto& content)
		{
			return content->lastDrawFenceValue <= completedFence;
		});

	for (std::size_t slotIndex = 0; slotIndex < m_contents.size(); ++slotIndex)
	{
		const auto handle = m_slots.GetDestroyRequestedHandle(slotIndex);
		if (handle == RenderTargetTextureHandle{})
		{
			continue;
		}

		const auto& content = m_contents[slotIndex];
		if (!content)
		{
			std::terminate();
		}

		if (content->lastDrawFenceValue > completedFence)
		{
			continue;
		}

		m_contents[slotIndex].reset();
		const auto released = m_slots.ReleaseSlotAfterResourceDestruction(handle);
		if (released != ResourceHandleValidationResult::Success)
		{
			std::terminate();
		}
	}
}

}
