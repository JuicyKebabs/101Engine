#pragma once

#include "Renderer/Texture.h"
#include "TextureCpuData.h"
#include "ResourceSlotTable.h"
#include "ResourceState.h"
#include "Backend/TextureGpuData.h"

#include <cstddef>
#include <exception>
#include <cstdint>
#include <memory>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

//---------------------------------------------------------------------------------
// Texture registry
// Registers and manages Texture resources for the rendering library.
// Resources are managed with the kinds of the array of data.
// One is the state management for the resource, and the other is the actual data.
//---------------------------------------------------------------------------------

namespace rendering::detail
{

struct TextureContent
{
	ResourceContentState state;
	TextureCpuData cpuData;
	std::unique_ptr<backend::TextureGpuData> gpuData;

	// Upload submission fence, not the last Draw fence. Zero means unsubmitted.
	std::uint64_t uploadFenceValue = 0;

	// Same Graphics Queue timeline; stamped only by actual Draw submissions
	std::uint64_t lastDrawFenceValue = 0;
};

class TextureRegistry
{
public:
	explicit TextureRegistry(std::uint32_t ownerId)
		: m_slots(ownerId)
	{}

	[[nodiscard]]
	TextureRegistrationResult Register(const TextureDesc& desc)
	{
		try
		{
			auto content = std::make_unique<TextureContent>();

			// Copy given Texture description to the actual CPU data.
			Result copied = CopyTextureCpuData(desc, content->cpuData);
			if (!copied.IsSuccess())
			{
				return { std::move(copied), {} };
			}

			// Get a registration slot for this Texture resource.
			const std::size_t index = m_slots.FindRegistrationSlotIndex();

			if (index >= TextureHandle::InvalidIndex || index >= m_contents.max_size())
			{
				return { {false, "Texture registration size is unsupported."}, {} };
			}

			// Allocate a new slot if the reusable slot is not found.
			if (index >= m_contents.size())
			{
				m_contents.resize(index + 1);
			}

			// Register the Texture resource and get a handle for it.
			auto registered = m_slots.Register();

			if (!registered.status.IsSuccess())
			{
				return { std::move(registered.status), {} };
			}

			// Store the Texture content in the corresponding slot.
			m_contents[registered.handle.slotIndex] = std::move(content);

			return { std::move(registered.status), registered.handle };
		}
		catch (const std::bad_alloc&)
		{
			return { {false, "Texture registration allocation failed."}, {} };
		}
		catch (const std::length_error&)
		{
			return { {false, "Texture registration size is unsupported."}, {} };
		}
	}

	[[nodiscard]]
	ResourceHandleValidationResult RequestDestroy(TextureHandle handle) noexcept
	{
		return m_slots.RequestDestroy(handle);
	}

	// Internal lookup; caller must retain the content and prevent collection.
	// This does not authorize new use or extend its lifetime.
	[[nodiscard]]
	TextureContent* FindRegisteredContent(TextureHandle handle) noexcept
	{
		if (m_slots.ValidateRegistered(handle) != ResourceHandleValidationResult::Success)
		{
			return nullptr;
		}

		return m_contents[handle.slotIndex].get();
	}

	// Pending content can be accepted.
	// GPU readiness is checked later.
	[[nodiscard]]
	const TextureContent* FindForNewUse(TextureHandle handle) const noexcept
	{
		if (m_slots.ValidateForNewUse(handle) != ResourceHandleValidationResult::Success)
		{
			return nullptr;
		}

		const auto* content = m_contents[handle.slotIndex].get();

		if (!content || content->state.preparationState == ResourcePreparationState::Failed)
		{
			return nullptr;
		}

		return content;
	}

	// Caller guarantees successful upload/transition Execute + Signal on this
	// renderer's Graphics Queue. Ready does not imply CPU-observed completion.
	[[nodiscard]]
	bool MarkReadyAfterUploadSubmission(TextureHandle handle, TextureContent& content, std::uint64_t fenceValue) noexcept
	{
		if (FindRegisteredContent(handle) != &content ||
			content.state.preparationState != ResourcePreparationState::Pending ||
			!content.gpuData || !content.gpuData->texture || !content.gpuData->GetSrvHandle().ptr ||
			fenceValue == 0 || fenceValue == (std::numeric_limits<std::uint64_t>::max)())
		{
			return false;
		}

		content.uploadFenceValue = fenceValue;
		content.state.preparationState = ResourcePreparationState::Ready;

		return true; // Does not clear Destroy requested after selection.
	}

	// Log first. Only ordinary preparation failures; fatal GPU errors use Backend policy.
	// Idempotent Failed notification; no content, descriptor or Upload is freed here.
	[[nodiscard]]
	bool MarkPreparationFailed(TextureHandle handle, TextureContent& content) noexcept
	{
		if (FindRegisteredContent(handle) != &content ||
			(content.state.preparationState != ResourcePreparationState::Pending &&
				content.state.preparationState != ResourcePreparationState::Failed))
		{
			return false;
		}

		if (m_slots.RequestDestroy(handle) != ResourceHandleValidationResult::Success)
		{
			return false;
		}

		content.state.preparationState = ResourcePreparationState::Failed;

		return true;
	}

	void CollectDestroyed(std::uint64_t completedGraphicsFence)
	{
		for (std::size_t index = 0; index < m_contents.size(); ++index)
		{
			const auto handle = m_slots.GetDestroyRequestedHandle(index);
			if (handle == TextureHandle{})
			{
				continue;
			}

			const auto& content = m_contents[index];
			if (!content)
			{
				std::terminate(); // Inconsistent state: destroy requested but no content.
			}

			// Recorder fence value for each content must be less than or equal to the completed fence value.
			if (content->uploadFenceValue > completedGraphicsFence ||
				content->lastDrawFenceValue > completedGraphicsFence)
			{
				continue;
			}

			// Release the content and its GPU resources.
			m_contents[index].reset();

			// Make the slot unregistered and increment its generation for future reuse.
			if (m_slots.ReleaseSlotAfterResourceDestruction(handle) != ResourceHandleValidationResult::Success)
			{
				std::terminate(); // Inconsistent state: failed to release slot after destruction.
			}
		}
	}

private:
	TextureSlotTable m_slots;									// State management for Texture resources.
	std::vector<std::unique_ptr<TextureContent>> m_contents;	// Actual Texture data.
};

}
