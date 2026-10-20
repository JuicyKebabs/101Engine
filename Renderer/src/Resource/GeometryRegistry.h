#pragma once

#include "Renderer/Geometry.h"
#include "GeometryCpuData.h"
#include "ResourceSlotTable.h"
#include "ResourceState.h"
#include "Backend/GeometryGpuData.h"

#include <cstddef>
#include <exception>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>
#include <limits>

//---------------------------------------------------------------------------------
// Geometry registry
// Registers and manages geometry resources for the rendering library.
// Resources are managed with the kinds of the array of data.
// One is the state management for the resource, and the other is the actual data.
//---------------------------------------------------------------------------------

namespace rendering::detail
{

struct GeometryContent
{
	ResourceContentState state;
	GeometryCpuData cpuData;
	std::unique_ptr<backend::GeometryGpuData> gpuData;

	// Fence value for upload submission (not for rendering this geometry)
	// Zero means not submitted yet.
	std::uint64_t uploadFenceValue = 0;

	// Same Graphics Queue timeline; stamped only by actual Draw submissions
	std::uint64_t lastDrawFenceValue = 0;
};

class GeometryRegistry
{
public:
	explicit GeometryRegistry(std::uint32_t ownerId)
		: m_slots(ownerId)
	{}

	[[nodiscard]]
	GeometryRegistrationResult Register(const GeometryDesc& desc)
	{
		try
		{
			auto content = std::make_unique<GeometryContent>();

			// Copy given geometry description to the actual CPU data.
			Result copied = CopyGeometryCpuData(desc, content->cpuData);
			if (!copied.IsSuccess())
			{
				return { std::move(copied), {} };
			}

			// Get a registration slot for this geometry resource.
			const std::size_t index = m_slots.FindRegistrationSlotIndex();

			if (index >= GeometryHandle::InvalidIndex || index >= m_contents.max_size())
			{
				return { {false, "Geometry registration size is unsupported."}, {} };
			}

			// Allocate a new slot if the reusable slot is not found.
			if (index >= m_contents.size())
			{
				m_contents.resize(index + 1);
			}

			// Register the geometry resource and get a handle for it.
			auto registered = m_slots.Register();
			if (!registered.status.IsSuccess())
			{
				return { std::move(registered.status), {} };
			}

			// Store the geometry content in the corresponding slot.
			m_contents[registered.handle.slotIndex] = std::move(content);

			return { std::move(registered.status), registered.handle };
		}
		catch (const std::bad_alloc&)
		{
			return { {false, "Geometry registration allocation failed."}, {} };
		}
		catch (const std::length_error&)
		{
			return { {false, "Geometry registration size is unsupported."}, {} };
		}
	}

	[[nodiscard]]
	ResourceHandleValidationResult RequestDestroy(GeometryHandle handle) noexcept
	{
		return m_slots.RequestDestroy(handle);
	}

	// Internal lookup; does not authorize new use or extend lifetime.
	// Caller must keep the content alive and prevent collection.
	[[nodiscard]]
	GeometryContent* FindRegisteredContent(GeometryHandle handle) noexcept
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
	const GeometryContent* FindForNewUse(GeometryHandle handle) const noexcept
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

	// Caller has successfully submitted upload work and signaled this renderer's Graphics Queue. 
	// This does not wait for GPU completion.
	[[nodiscard]]
	bool MarkReadyAfterUploadSubmission(
		GeometryHandle handle,
		GeometryContent& content,
		std::uint64_t fenceValue) noexcept
	{
		bool finded = FindRegisteredContent(handle) == &content;
		bool validState = content.state.preparationState == ResourcePreparationState::Pending;
		bool validResource = content.gpuData && content.gpuData->vertices.GetNativeResource();
		bool validIndices = !!content.cpuData.indices.empty() ? true : (content.gpuData && content.gpuData->indices.GetNativeResource());
		bool validFence = fenceValue != 0 && fenceValue != (std::numeric_limits<std::uint64_t>::max)();
		
		if (!finded || !validState || !validResource || !validIndices || !validFence)
		{
			return false;
		}

		content.uploadFenceValue = fenceValue;
		content.state.preparationState = ResourcePreparationState::Ready;
		return true;
	}

	// Caller logs the failure before collection. No resources are freed here (Just get marked for destruction).
	// Ordinary preparation failures only; fatal GPU failures use Backend's policy.
	[[nodiscard]]
	bool MarkPreparationFailed(
		GeometryHandle handle,
		GeometryContent& content) noexcept
	{
		bool finded = FindRegisteredContent(handle) == &content;
		bool validState = 
			content.state.preparationState == ResourcePreparationState::Pending ||
			content.state.preparationState == ResourcePreparationState::Failed;

		if (!finded || !validState)
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

			if (handle == GeometryHandle{})
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
	GeometrySlotTable m_slots;									// State management for geometry resources.
	std::vector<std::unique_ptr<GeometryContent>> m_contents;	// Actual geometry data (CPU data and GPU resources).
};

}
