#include "TextureReadbackRegistry.h"
#include "Backend/Backend.h"
#include "Failure.h"
#include "RenderTargetTextureRegistry.h"

#include <exception>
#include <utility>

namespace rendering::detail
{

namespace
{

constexpr std::size_t MaxReadbackRequests = 32;
constexpr std::size_t MaxReadbackBytes = 128 * 1024 * 1024;

}

ReadbackRequestResult TextureReadbackRegistry::Register(
	backend::Backend& backend,
	const RenderTargetTextureRegistry& renderTargetRegistry,
	const TextureReadbackDesc& desc)
{
	// Get the target texture of the readback request and validate its availability.
	const auto* source = renderTargetRegistry.FindForNewUse(desc.sourceTexture);

	ReadbackRequestResult accepted;

	if (!source || !source->gpuData)
	{
		accepted.status = { false, "RegisterTextureReadback: source texture is not available." };
		return accepted;
	}

	const bool validTiming =
		desc.timing == ReadbackTiming::BeforeRendering ||
		desc.timing == ReadbackTiming::AfterRendering;
	const bool validRevision = source->revision == desc.expectedSourceRevision;

	if (!validTiming || !validRevision)
	{
		accepted.status = { false, "RegisterTextureReadback: invalid timing or source revision." };
		return accepted;
	}

	// Create new readback request and plan the transfer.
	auto candidate = std::make_unique<TextureReadbackRequest>();

	// Copy the request description into the transfer of new request.
	accepted.status = backend.PlanTextureReadback(*source->gpuData, desc.region, candidate->transfer);

	if (!accepted.status.IsSuccess())
	{
		return accepted;
	}

	// Check if the new request can be accepted based on the
	// current number of requests and reserved bytes.
	std::size_t requestCount = 0;
	std::size_t reservedBytes = 0;

	for (const auto& request : m_requests)
	{
		if (request)
		{
			requestCount++;
			reservedBytes += request->transfer.ReservedBytes();
		}
	}

	if (requestCount >= MaxReadbackRequests ||
		candidate->transfer.ReservedBytes() > MaxReadbackBytes - reservedBytes)
	{
		accepted.status = { false, "RegisterTextureReadback: readback request limits exceeded." };
		return accepted;
	}

	candidate->desc = desc;
	candidate->data.format = source->creationDescription.format;
	candidate->data.width = desc.region.width;
	candidate->data.height = desc.region.height;
	candidate->data.rowPitch = candidate->transfer.RowBytes();

	// Find the next available slot for the new request and register it.
	const auto slotIndex = m_slots.FindRegistrationSlotIndex();

	if (slotIndex >= m_requests.size())
	{
		m_requests.resize(slotIndex + 1);
	}

	// Register the new request in the slot table.
	auto registration = m_slots.Register();
	accepted.status = std::move(registration.status);

	// Store the request itself in the request table.
	if (accepted.status.IsSuccess())
	{
		m_requests[slotIndex] = std::move(candidate);
		accepted.handle = registration.handle;
	}

	return accepted;
}

ReadbackPollResult TextureReadbackRegistry::Take(
	ReadbackHandle handle,
	TextureReadbackData& output)
{
	ReadbackPollResult result;

	if (m_slots.ValidateForNewUse(handle) != ResourceHandleValidationResult::Success)
	{
		result.state = ReadbackState::Failed;
		result.error = Fail("TakeTextureReadback: invalid readback handle.");
		return result;
	}

	auto& request = *m_requests[handle.slotIndex];
	result.state = request.state;

	// Move the readback data to the output if the request is ready.
	if (request.state == ReadbackState::Ready)
	{
		output = std::move(request.data);
		result.error = Release(handle);

		if (!result.error.IsSuccess())
		{
			result.state = ReadbackState::Failed;
		}

		return result;
	}

	result.error = request.error;

	if (request.state == ReadbackState::Failed && result.error.message.empty())
	{
		result.error.message = "TakeTextureReadback: readback request failed but no error message was provided.";
	}

	return result;
}

Result TextureReadbackRegistry::Release(ReadbackHandle handle)
{
	if (m_slots.RequestDestroy(handle) != ResourceHandleValidationResult::Success)
	{
		return Fail("ReleaseTextureReadback: invalid readback handle.");
	}

	m_requests[handle.slotIndex].reset();

	if (m_slots.ReleaseSlotAfterResourceDestruction(handle) != ResourceHandleValidationResult::Success)
	{
		std::terminate();
	}

	return  {};
}

Result TextureReadbackRegistry::CompleteFrame(backend::Backend& backend)
{
	for (auto& request : m_requests)
	{
		if (!request ||
			request->state != ReadbackState::Pending ||
			!request->transfer.HasBuffer())
		{
			continue;
		}

		// Record the texture readback operation for the request.
		std::vector<std::byte> pixels(request->transfer.PackedBytes());
		Result result = backend.ReadTextureReadback(request->transfer, pixels);

		if (!result.IsSuccess())
		{
			if (result.failureKind != FailureKind::General)
			{
				return result;
			}

			request->state = ReadbackState::Failed;
			request->error = std::move(result);

			continue;
		}

		// Move the readback data into the request and mark it as ready.
		request->data.pixels = std::move(pixels);
		request->state = ReadbackState::Ready;
	}

	return {};
}

void TextureReadbackRegistry::FailPendingRequests(const Result& failure) noexcept
{
	for (auto& request : m_requests)
	{
		if (request && request->state == ReadbackState::Pending)
		{
			request->state = ReadbackState::Failed;
			request->error.succeeded = false;
			request->error.failureKind = failure.failureKind;
		}
	}
}

void TextureReadbackRegistry::ReleaseFrameBuffers() noexcept
{
	for (auto& request : m_requests)
	{
		if (request)
		{
			request->transfer.ReleaseBuffer();
		}
	}
}

}
