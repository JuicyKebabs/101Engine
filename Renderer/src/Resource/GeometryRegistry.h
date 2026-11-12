#pragma once

#include "Renderer/Geometry.h"
#include "GeometryCpuData.h"
#include "ResourceSlotTable.h"
#include "ResourceState.h"
#include "Backend/GeometryGpuData.h"
#include "Renderer/DrawTypes.h"

#include <exception>
#include <cstdint>
#include <memory>
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
};

class GeometryRegistry
{
public:
	std::string DescribeUnavailable(GeometryHandle handle) const
	{
		return DescribeResourceIdentity(handle, m_slots.ValidateForNewUse(handle));
	}

	explicit GeometryRegistry(std::uint32_t ownerId) : m_slots(ownerId) {}

	[[nodiscard]]
	ResourceRegistrationResult<GeometryHandle> Register(const GeometryDesc& desc)
	{
		try
		{
			auto content = std::make_unique<GeometryContent>();

			// Copy given geometry description to the actual CPU data.
			Result result = CopyGeometryCpuData(desc, content->cpuData);

			if (!result.IsSuccess())
			{
				return {std::move(result), {}};
			}

			// Get a registration slot for this geometry resource.
			const std::size_t index = m_slots.FindRegistrationSlotIndex();

			if (index >= GeometryHandle::InvalidIndex || index >= m_contents.max_size())
			{
				return {{false, "Geometry registration size is unsupported."}, {}};
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
				return {std::move(registered.status), {}};
			}

			// Store the geometry content in the corresponding slot.
			m_contents[registered.handle.slotIndex] = std::move(content);

			return {std::move(registered.status), registered.handle};
		}
		catch (const std::bad_alloc&)
		{
			return {{false, "Geometry registration allocation failed."}, {}};
		}
		catch (const std::length_error&)
		{
			return {{false, "Geometry registration size is unsupported."}, {}};
		}
	}

	[[nodiscard]]
	Result Destroy(GeometryHandle handle) noexcept
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

	// Called only after the upload batch has completed on the GPU.
	[[nodiscard]]
	bool MarkPrepared(GeometryHandle handle, GeometryContent& content) noexcept
	{
		const bool owned = OwnsContent(handle, &content);
		const bool pending = content.state.preparationState == ResourcePreparationState::Pending;
		const bool validVertices = content.gpuData && content.gpuData->vertices.GetNativeResource();
		const bool validIndices =
			content.cpuData.indices.empty() || (content.gpuData && content.gpuData->indices.GetNativeResource());
		const bool validBuffers = validVertices && validIndices;

		if (!owned || !pending || !validBuffers)
		{
			return false;
		}

		content.state.preparationState = ResourcePreparationState::Ready;
		return true;
	}

	// Caller logs the failure before collection. No resources are freed here (Just get marked for destruction).
	// Ordinary preparation failures only; fatal GPU failures use Backend's policy.
	[[nodiscard]]
	bool MarkPreparationFailed(GeometryHandle handle, GeometryContent& content) noexcept
	{
		const bool owned = OwnsContent(handle, &content);
		const bool validState = content.state.preparationState == ResourcePreparationState::Pending ||
								content.state.preparationState == ResourcePreparationState::Failed;

		if (!owned || !validState)
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
			return {false, "update target unavailable"};
		}

		// Get current content for the given handle
		auto& current = m_contents[handle.slotIndex];

		if (current->revision == (std::numeric_limits<std::uint64_t>::max)())
		{
			return {false, "update target revision is unavailable"};
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

		// Render has completed before public updates can replace this owner.
		current = std::move(candidate);

		return {};
	}

	// Check if the registry owns the given content for the specified handle.
	// Render forbids replacement while any command preparation borrows exist.
	bool OwnsContent(GeometryHandle handle, const GeometryContent* content) const noexcept
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

			if (handle != GeometryHandle{})
			{
				ReleaseContent(handle);
			}
		}
	}

private:
	void ReleaseContent(GeometryHandle handle) noexcept
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

	GeometrySlotTable m_slots;									// State management for geometry resources.
	std::vector<std::unique_ptr<GeometryContent>> m_contents;	// Actual geometry data (CPU data and GPU resources).
};

} // namespace rendering::detail
