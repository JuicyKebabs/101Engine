// R7 implementation guide: section 4.16.
// Resolve and commit through the real owner rather than caching detached descriptors.
// Preflight re-queries the same resource that was declared; replacement during Render is forbidden.
// State and initialized contents are separate facts and are committed only after successful submission.

#include "FrameTextureResolver.h"
#include "Resource/TextureRegistry.h"
#include "Resource/RenderTargetTextureRegistry.h"
#include "Backend/DepthTexture.h"
#include "Failure.h"

namespace rendering::graph
{

// Re-query the declared owner to verify that live identity and descriptors still match the import.
// This is validation of a borrow, not permission to silently switch to a resized resource during Execute.
Result QueryBinding(
    backend::Backend& backend,
    const GraphTextureBinding& borrowed,
    GraphTextureBinding& output)
{
    output = borrowed;

    if (std::holds_alternative<std::monostate>(borrowed.owner))
    {
        backend::BackBufferInfo current;
        auto result = backend.QueryTextureInfo(borrowed.Resource(), current);

        if (!result.IsSuccess())
        {
            return result;
        }

        output.resource = current.Resource();
        output.rtv = current.Rtv();
        output.viewFormat = current.ViewFormat();
        output.initialState = current.SubmittedState();
        output.presentable = true;
        output.contentsValid = false;
        output.srv = {};
        output.dsv = {};
    }
	else if (auto content = std::get_if<detail::TextureContent*>(&borrowed.owner))
	{
		if (!*content || !(*content)->gpuData ||
			(*content)->state.preparationState != detail::ResourcePreparationState::Ready)
		{
			return { false, "QueryBinding: registered Texture version is not ready" };
		}

		auto& gpu = *(*content)->gpuData;
		output.resource = gpu.texture.Get();
		output.srv = gpu.GetSrvHandle();
		output.rtv = {};
		output.dsv = {};
		output.presentable = false;
		output.viewFormat = gpu.texture->GetDesc().Format;
		output.initialState = gpu.submittedState;
		output.contentsValid = gpu.contentsValid;
	}
	else if (auto content = std::get_if<detail::RenderTargetTextureContent*>(&borrowed.owner))
	{
		if (!*content || !(*content)->gpuData)
		{
			return detail::Fail("QueryBinding: RenderTargetTexture owner is unavailable.");
		}

		auto& gpu = *(*content)->gpuData;
		output.resource = gpu.Resource();
		output.rtv = gpu.Rtv();
		output.srv = gpu.Srv();
		output.dsv = {};
		output.presentable = false;
		output.viewFormat = gpu.viewFormat;
		output.initialState = gpu.submittedState;
		output.contentsValid = gpu.contentsValid;
	}
	else
	{
		auto* depth = std::get<backend::DepthTexture*>(borrowed.owner);

		if (!depth || !depth->Resource())
		{
			return { false, "QueryBinding: Depth owner unavailable" };
		}

		output.resource = depth->Resource();
		output.dsv = depth->Dsv();
		output.rtv = {};
		output.srv = depth->Srv();
		output.presentable = false;
		output.viewFormat = DXGI_FORMAT_D32_FLOAT;
		output.initialState = depth->submittedState;
		output.contentsValid = depth->contentsValid;
	}

	if (!output.resource || output.resource != borrowed.resource)
	{
		return { false, "QueryBinding: Texture resource mismatch" };
	}

	return {};
}

// Called only after Execute/Signal succeeds, using identities validated in Preflight.
// Publish submitted State and content validity back to the same physical owner.
// The use Fence is stamped by the executor; this function itself does not wait for completion.
void CommitBinding(backend::Backend& backend,
    const GraphTextureBinding& binding,
    D3D12_RESOURCE_STATES state,
	bool contentsValid) noexcept
{
	if (std::holds_alternative<std::monostate>(binding.owner))
	{
		backend.CommitTextureState(binding.resource, state);
	}
	else if (auto content = std::get_if<detail::TextureContent*>(&binding.owner))
	{
		(*content)->gpuData->submittedState = state;
		(*content)->gpuData->contentsValid = contentsValid;
	}
	else if (auto content = std::get_if<detail::RenderTargetTextureContent*>(&binding.owner))
	{
		(*content)->gpuData->submittedState = state;
		(*content)->gpuData->contentsValid = contentsValid;
	}
	else
	{
		auto* depth = std::get<backend::DepthTexture*>(binding.owner);
		depth->submittedState = state;
		depth->contentsValid = contentsValid;
	}
}

}
