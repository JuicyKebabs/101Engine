#pragma once

#include "Renderer/Texture.h"
#include "TextureCpuData.h"
#include "ResourceSlotTable.h"
#include "ResourceState.h"
#include "Backend/TextureGpuData.h"

#include <cstddef>
#include <algorithm>
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
	std::uint64_t revision = 1;
	TextureCpuData cpuData;
	std::unique_ptr<backend::TextureGpuData> gpuData;
};

class TextureRegistry
{
public:
	std::string DescribeUnavailable(TextureHandle handle) const
	{
		return DescribeResourceIdentity(handle, m_slots.ValidateForNewUse(handle));
	}

	explicit TextureRegistry(std::uint32_t ownerId) : m_slots(ownerId) {}

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
				return {std::move(copied), {}};
			}

			// Get a registration slot for this Texture resource.
			const std::size_t index = m_slots.FindRegistrationSlotIndex();

			if (index >= TextureHandle::InvalidIndex || index >= m_contents.max_size())
			{
				return {{false, "Texture registration size is unsupported."}, {}};
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
				return {std::move(registered.status), {}};
			}

			// Store the Texture content in the corresponding slot.
			m_contents[registered.handle.slotIndex] = std::move(content);

			return {std::move(registered.status), registered.handle};
		}
		catch (const std::bad_alloc&)
		{
			return {{false, "Texture registration allocation failed."}, {}};
		}
		catch (const std::length_error&)
		{
			return {{false, "Texture registration size is unsupported."}, {}};
		}
	}

	[[nodiscard]]
	Result Destroy(TextureHandle handle) noexcept
	{
		if (m_slots.RequestDestroy(handle) != ResourceHandleValidationResult::Success)
		{
			return { false, "invalid destruction handle" };
		}

		m_contents[handle.slotIndex].reset();

		if (m_slots.ReleaseSlotAfterResourceDestruction(handle) != ResourceHandleValidationResult::Success)
		{
			std::terminate();
		}

		return {};
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

	// Called only after the upload batch has completed on the GPU.
	[[nodiscard]]
	bool MarkPrepared(TextureHandle handle, TextureContent& content) noexcept
	{
		const bool pending = content.state.preparationState == ResourcePreparationState::Pending;
		const bool validTexture = content.gpuData && content.gpuData->texture && content.gpuData->GetSrvHandle().ptr;

		if (!OwnsContent(handle, &content) || !pending || !validTexture)
		{
			return false;
		}

		content.gpuData->submittedState =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		content.gpuData->contentsValid = true;
		content.state.preparationState = ResourcePreparationState::Ready;

		return true;
	}

	// Log first. Only ordinary preparation failures; fatal GPU errors use Backend policy.
	// Idempotent Failed notification; no content, descriptor or Upload is freed here.
	[[nodiscard]]
	bool MarkPreparationFailed(TextureHandle handle, TextureContent& content) noexcept
	{
		if (!OwnsContent(handle, &content) || (content.state.preparationState != ResourcePreparationState::Pending &&
                                               content.state.preparationState != ResourcePreparationState::Failed))
		{
			return false;
		}

		if (FindRegisteredContent(handle) == &content &&
			m_slots.RequestDestroy(handle) != ResourceHandleValidationResult::Success)
		{
			return false;
		}

		content.state.preparationState = ResourcePreparationState::Failed;

		return true;
	}

	// Update the content for the given handle with a new texture description.
	Result Update(TextureHandle handle, const TextureDesc& desc)
	{
		if (!FindForNewUse(handle))
		{
			return {false, "update target unavailable"};
		}

		// Get current content for the given handle
		auto& current = m_contents[handle.slotIndex];

		if (current->revision == (std::numeric_limits<std::uint64_t>::max)())
		{
			return {false, "update target revision is unavailable"};
		}

		// Generate a new content and copy the given texture description to it.
		auto candidate = std::make_unique<TextureContent>();
		Result copied = CopyTextureCpuData(desc, candidate->cpuData);

		if (!copied.IsSuccess())
		{
			return copied;
		}

		// Increment the revision number for the new candidate content.
		candidate->revision = current->revision + 1;

		// Render has completed before public updates can replace this owner.
		current = std::move(candidate);

		return {};
	}

	// Check if the registry owns the given content for the specified handle.
	// Render forbids replacement while any command preparation borrows exist.
	bool OwnsContent(TextureHandle handle, const TextureContent* content) const noexcept
	{
		return content && m_slots.ValidateRegistered(handle) == ResourceHandleValidationResult::Success &&
               m_contents[handle.slotIndex].get() == content;
	}

	void CollectDestroyed()
	{
		// Only failed preparation can defer destruction until Render's temporary borrows end.
		for (std::size_t index = 0; index < m_contents.size(); ++index)
		{
			const auto handle = m_slots.GetDestroyRequestedHandle(index);

			if (handle != TextureHandle{})
			{
				ReleaseContent(handle);
			}
		}
	}

private:
	void ReleaseContent(TextureHandle handle) noexcept
	{
		if (!m_contents[handle.slotIndex])
		{
			std::terminate();
		}

		m_contents[handle.slotIndex].reset();

		if (m_slots.ReleaseSlotAfterResourceDestruction(handle) != ResourceHandleValidationResult::Success)
		{
			std::terminate();
		}
	}

	TextureSlotTable m_slots;                                // State management for Texture resources.
	std::vector<std::unique_ptr<TextureContent>> m_contents; // Actual Texture data.
};

} // namespace rendering::detail
