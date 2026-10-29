#pragma once

#include "Renderer/Geometry.h"
#include "GeometryCpuData.h"
#include "ResourceSlotTable.h"
#include "ResourceState.h"
#include "Backend/GeometryGpuData.h"

#include <cstddef>
#include <algorithm>
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
	std::uint64_t revision = 1;
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
    std::string DescribeUnavailable(GeometryHandle handle) const
    {
        return DescribeResourceIdentity(handle, m_slots.ValidateForNewUse(handle));
    }

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

    // Query one accepted Content without exposing its address or GPU resources.
    // Failed lookup leaves the caller's previous snapshot untouched.
    Result GetData(GeometryHandle handle, GeometryData& output) const
    {
        const GeometryContent* content = FindForNewUse(handle);
        if (!content)
        {
            Result failure;
            failure.succeeded = false;
            failure.message = "GeometryData target is invalid, destroyed, failed, or belongs to another Renderer.";
            return failure;
        }

        GeometryData snapshot;
        snapshot.boundsCenter = content->cpuData.boundsCenter;
        snapshot.boundsRadius = content->cpuData.boundsRadius;
        snapshot.revision = content->revision;
        output = snapshot;
        return {};
    }

	// Caller has successfully submitted upload work and signaled this renderer's Graphics Queue. 
	// This does not wait for GPU completion.
	[[nodiscard]]
	bool MarkReadyAfterUploadSubmission(
		GeometryHandle handle,
		GeometryContent& content,
		std::uint64_t fenceValue) noexcept
	{
		bool finded = OwnsContent(handle, &content);
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
		bool finded = OwnsContent(handle, &content);	// Include retired contents.
		bool validState = 
			content.state.preparationState == ResourcePreparationState::Pending ||
			content.state.preparationState == ResourcePreparationState::Failed;

		if (!finded || !validState)
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

	// Update the CPU data of the geometry resource for the given handle with the new geometry description.
	Result Update(GeometryHandle handle, const GeometryDesc& desc)
	{
		if (!FindForNewUse(handle))
		{
			return { false, "update target unavailable" };
		}

		// Get current content for the given handle
		auto& current = m_contents[handle.slotIndex];

		if (current->revision == (std::numeric_limits<std::uint64_t>::max)())
		{
			return { false, "update target revision is unavailable" };
		}

		// Generate a new content and copy the given geometry description to it.
		auto candidate = std::make_unique<GeometryContent>();
		Result copied = CopyGeometryCpuData(desc, candidate->cpuData);

		if (!copied.IsSuccess())
		{
			return copied;
		}

		// Increment the revision number for the new candidate content.
		candidate->revision = current->revision + 1;

		// Swap the current content with the candidate
		// and move the current content to the retired list.
		m_retired.emplace_back();
		m_retired.back().handle = handle;
		m_retired.back().content = std::move(current);
		current = std::move(candidate);

		return {};
	}

	// Check if the registry owns the given content for the specified handle.
	// (search in both registered and retired contents)
	bool OwnsContent(GeometryHandle handle, const GeometryContent* content) const noexcept
	{
		if (!content)
		{
			return false;
		}

		if (m_slots.ValidateRegistered(handle) == ResourceHandleValidationResult::Success &&
			m_contents[handle.slotIndex].get() == content)
		{
			return true;
		}

		// Check if the content is in the retired list.
		return std::any_of(m_retired.begin(), m_retired.end(), [&](const auto& x)
			{
				return x.handle == handle && x.content.get() == content;
			}
		);
	}

	void CollectRetired(std::uint64_t completed)
	{
		// Erase the retired handles if it's no more used by GPU (lastDrawFenceValue <= completed).
		std::erase_if(m_retired, [&](const auto& x)
			{
				return x.content->uploadFenceValue <= completed && x.content->lastDrawFenceValue <= completed;
			}
		);
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

	struct Retired
	{
		GeometryHandle handle;
		std::unique_ptr<GeometryContent> content;
	};

	std::vector<Retired> m_retired;	// Store replaced contents for GPU completion before collection.
};

}
