#pragma once

#include "Backend/TextureReadbackTransfer.h"
#include "ResourceSlotTable.h"

#include <memory>
 #include <vector>

//------------------------------------------------------------------------------------------------
// TextureReadbackRegistry
// The registry stores the every requests for texture readback.
// Keep the request detail and the transfer information
// for the backend to perform the readback operation inTextureReadbackRequest.
// The data of the readback is also stored in TextureReadbackData when the readback is completed.
//------------------------------------------------------------------------------------------------

namespace rendering::detail
{

class RenderTargetTextureRegistry;

struct TextureReadbackRequest
{
	TextureReadbackDesc desc;						// The target texture(handle) and region to read back.
	ReadbackState state = ReadbackState::Pending;	// The current state of the readback request.
	Result error;									// Used ehwn the state get to Failed.
	TextureReadbackData data;						// Store the readback data when the state get to Ready.
	backend::TextureReadbackTransfer transfer;		// Backend transfer information for the readback operation.
};

class TextureReadbackRegistry
{
public:
	explicit TextureReadbackRegistry(std::uint32_t ownerId) : m_slots(ownerId) {}

	ReadbackRequestResult Register(
		backend::Backend& backend,
		const RenderTargetTextureRegistry& renderTargetRegistry,
		const TextureReadbackDesc& desc);

	// Check the state of the given readback request and return the result.
	// Used in TryTakeTextureReadback() which is publicly exposed to the User. 
	ReadbackPollResult Take(ReadbackHandle handle, TextureReadbackData& output);

	Result Release(ReadbackHandle handle);

	// Store the readback pixel data to the request and mark it as Ready.
	Result CompleteFrame(backend::Backend& backend);

	void FailPendingRequests(const Result& failure) noexcept;
	void ReleaseFrameBuffers() noexcept;

	// Iterates over all pending readback requests with the 
	// specified timing and applies the provided function.
	template<class Function>
	Result ForEachPending(ReadbackTiming timing, Function&& function)
	{
		for (auto& request : m_requests)
		{
			if (!request ||
				request->state != ReadbackState::Pending ||
				request->desc.timing != timing)
			{
				continue;
			}

			Result result = function(*request);

			if (!result.IsSuccess())
			{
				return result;
			}
		}

		return {};
	}

private:
	ResourceSlotTable<ReadbackHandle> m_slots;
	std::vector<std::unique_ptr<TextureReadbackRequest>> m_requests;

};

}
