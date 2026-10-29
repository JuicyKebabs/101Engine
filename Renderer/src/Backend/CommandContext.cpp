// R7 implementation guide: section 4.28.
// Record native state only while the command list is in its recording window.
// Viewport coordinates are validated against the current output extent, not a cached Window size.
// This file does not decide Engine Canvas coordinates, DPI semantics or Sky ownership.

#include "CommandContext.h"
#include "Renderer/FrameRenderData.h"
#include "Failure.h"
#include <cmath>
#include "ApiResult.h"

#include <utility>
#include <algorithm>

namespace rendering::backend
{

using Microsoft::WRL::ComPtr;

CommandContext::~CommandContext()
{
	Finalize();
}

Result CommandContext::Initialize(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type)
{
	if (m_state != State::Uninitialized)
	{
		return { false, "CommandContext is already initialized." };
	}

	if (device == nullptr)
	{
		return { false, "CommandContext requires a valid device." };
	}

	// Create the command allocator and command list in local scope
	// to ensure proper cleanup in case of failure.
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> commandList;

	// Create the command allocator
	HRESULT hr = device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator));

	if (FAILED(hr))
	{
		return MakeApiFailure("CreateCommandAllocator", hr);
	}

	// Create the command list
	hr = device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));

	if (FAILED(hr))
	{
		return MakeApiFailure("CreateCommandList", hr);
	}

	// Close the command list initially, as it starts in the recording state.
	hr = commandList->Close();

	if (FAILED(hr))
	{
		return MakeApiFailure("ID3D12GraphicsCommandList::Close", hr);
	}

	// Move the created objects into the member variables
	m_commandAllocator = std::move(commandAllocator);
	m_commandList = std::move(commandList);
	m_state = State::Closed;

	OutputDebugStringW(L"[101Renderer] CommandContext initialized.\n");

	return {};
}

void CommandContext::Finalize() noexcept
{
	m_commandList.Reset();
	m_commandAllocator.Reset();
	m_state = State::Uninitialized;
}

Result CommandContext::Begin()
{
	if (m_state != State::Closed)
	{
		return { false, "Begin : CommandContext must be closed before Begin." };
	}

	// Reset the command allocator
	// All GPU commands must be completed before resetting the command allocator.
	HRESULT hr = m_commandAllocator->Reset();

	if (FAILED(hr))
	{
		m_state = State::Unusable;
		return MakeApiFailure("ID3D12CommandAllocator::Reset", hr);
	}

	// Reset the command list
	hr = m_commandList->Reset(m_commandAllocator.Get(), nullptr);

	if (FAILED(hr))
	{
		m_state = State::Unusable;
		return MakeApiFailure("ID3D12GraphicsCommandList::Reset", hr);
	}

	// Ready to record commands
	m_state = State::Recording;

	return {};
}

Result CommandContext::Close()
{
	if (m_state != State::Recording)
	{
		return { false, "CommandContext is not recording." };
	}

	const HRESULT hr = m_commandList->Close();

	if (FAILED(hr))
	{
		m_state = State::Unusable;
		return MakeApiFailure("ID3D12GraphicsCommandList::Close", hr);
	}

	// Command list is now closed and ready for execution
	m_state = State::Closed;

	return {};
}

Result CommandContext::TransitionResource(
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES stateBefore,
	D3D12_RESOURCE_STATES stateAfter,
	UINT subresource)
{
	if (!IsRecording())
	{
		return { false, "TransitionResource: CommandContext is not recording." };
	}

	if (resource == nullptr)
	{
		return { false, "TransitionResource: Resource is null." };
	}

	// No transition is needed when the states are identical.
	if (stateBefore == stateAfter)
	{
		return {};
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

	barrier.Transition.pResource = resource;
	barrier.Transition.Subresource = subresource;
	barrier.Transition.StateBefore = stateBefore;
	barrier.Transition.StateAfter = stateAfter;

	m_commandList->ResourceBarrier(1, &barrier);

	return {};
}

Result CommandContext::SetRasterOutput(
	std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> colors,
	UINT width,
	UINT height,
	D3D12_CPU_DESCRIPTOR_HANDLE depth)
{
	if (!IsRecording() || m_commandList->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		return { false, "SetRasterOutput: recording DIRECT context required." };
	}

	if (m_commandList->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		return { false, "SetRasterOutput: A direct command list is required." };
	}

	if (colors.empty() && !depth.ptr)
	{
		return { false, "SetRasterOutput: At least one color or depth descriptor is required." };
	}

	if (colors.size() > 1 || (!colors.empty() && !colors.front().ptr))
	{
		return { false, "SetRasterOutput: unsupported color count or descriptor." };
	}

	if (width == 0 ||
		height == 0 ||
		width > static_cast<UINT>(D3D12_VIEWPORT_BOUNDS_MAX) ||
		height > static_cast<UINT>(D3D12_VIEWPORT_BOUNDS_MAX))
	{
		return { false, "SetRasterOutput: unsupported extent." };
	}

	// Set the viewport and scissor rectangle to cover the entire render target.
	m_commandList->OMSetRenderTargets(static_cast<UINT>(colors.size()), colors.data(), FALSE, depth.ptr ? &depth : nullptr);

	const D3D12_VIEWPORT viewport{
		0.0f, 0.0f,
		static_cast<float>(width),
		static_cast<float>(height),
		0.0f, 1.0f
	};

	const D3D12_RECT scissorRect{
		0, 0,
		static_cast<LONG>(width),
		static_cast<LONG>(height)
	};

	m_commandList->RSSetViewports(1, &viewport);
	m_commandList->RSSetScissorRects(1, &scissorRect);

	return {};
}

// Viewport belongs to the active View and current output extent.
// Validate finite positive dimensions and containment before converting to the native rasterizer state.
// Scissor is separate and will be set per Draw rather than inherited from this operation.
Result CommandContext::SetViewport(const Viewport& viewport, UINT outputWidth, UINT outputHeight)
{
    if (!IsRecording())
    {
        return detail::Fail("Viewport requires an active recording.");
    }
    const bool finitePosition = std::isfinite(viewport.x) && std::isfinite(viewport.y);
    const bool finiteSize = std::isfinite(viewport.width) && std::isfinite(viewport.height);
    if (!finitePosition || !finiteSize)
    {
        return detail::Fail("Viewport must be finite.");
    }
    const bool validPosition = viewport.x >= 0 && viewport.y >= 0;
    const bool validSize = viewport.width > 0 && viewport.height > 0;
    if (!validPosition || !validSize)
    {
        return detail::Fail("Viewport position and dimensions are invalid.");
    }
    const bool insideWidth = viewport.x + viewport.width <= static_cast<float>(outputWidth);
    const bool insideHeight = viewport.y + viewport.height <= static_cast<float>(outputHeight);
    if (!insideWidth || !insideHeight)
    {
        return detail::Fail("Viewport exceeds current output dimensions (possibly after Resize).");
    }

    D3D12_VIEWPORT native;
    native.TopLeftX = viewport.x;
    native.TopLeftY = viewport.y;
    native.Width = viewport.width;
    native.Height = viewport.height;
    native.MinDepth = 0;
    native.MaxDepth = 1;
    m_commandList->RSSetViewports(1, &native);
    return {};
}

Result CommandContext::ClearRenderTarget(
	D3D12_CPU_DESCRIPTOR_HANDLE rtv,
	const std::array<float, 4>& color)
{
	if (!IsRecording())
	{
		return { false, "ClearRenderTarget: CommandContext is not recording." };
	}

	if (m_commandList->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		return { false, "ClearRenderTarget: A direct command list is required." };
	}

	if (rtv.ptr == 0)
	{
		return { false, "ClearRenderTarget: RTV handle is empty." };
	}

	m_commandList->ClearRenderTargetView(rtv, color.data(), 0, nullptr);

	return {};
}

Result CommandContext::CopyTextureRegion(
	ID3D12Resource* destination,
	UINT mip,
	ID3D12Resource* source,
	const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout)
{
	if (!IsRecording() || !destination || !source || destination == source)
	{
		return { false, "CopyTextureRegion requires recording and valid resources." };
	}

	const auto dst = destination->GetDesc();
	const auto src = source->GetDesc();

	if (dst.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		dst.DepthOrArraySize != 1 || dst.SampleDesc.Count != 1 ||
		(dst.Format != DXGI_FORMAT_R8G8B8A8_UNORM && dst.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) ||
		src.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || mip >= dst.MipLevels || mip >= 32)
	{
		return { false, "CopyTextureRegion requires an RGBA8 2D mip and buffer." };
	}

	const auto& footprint = layout.Footprint;
	const UINT64 width = std::max<UINT64>(1, dst.Width >> mip);
	const UINT height = std::max(1u, dst.Height >> mip);
	const UINT64 rowBytes = width * 4;

	if (footprint.Format != dst.Format || footprint.Width != width ||
		footprint.Height != height || footprint.Depth != 1 ||
		footprint.RowPitch < rowBytes ||
		footprint.RowPitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT != 0 ||
		layout.Offset % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT != 0 ||
		layout.Offset > src.Width || rowBytes > src.Width - layout.Offset ||
		height - 1 > (src.Width - layout.Offset - rowBytes) / footprint.RowPitch)
	{
		return { false, "CopyTextureRegion footprint is inconsistent or out of bounds." };
	}

	D3D12_TEXTURE_COPY_LOCATION to{};
	to.pResource = destination;
	to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	to.SubresourceIndex = mip;

	D3D12_TEXTURE_COPY_LOCATION from{};
	from.pResource = source;
	from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	from.PlacedFootprint = layout;
	m_commandList->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);

	return {};
}

Result CommandContext::CopyBufferRegion(
	ID3D12Resource* destination,
	std::uint64_t destinationOffset,
	ID3D12Resource* source,
	std::uint64_t sourceOffset,
	std::uint64_t byteCount)
{
	if (!IsRecording())
	{
		return { false, "CopyBufferRegion: CommandContext is not recording." };
	}

	if (!destination || !source || destination == source || byteCount == 0)
	{
		return { false, "CopyBufferRegion has invalid arguments." };
	}

	const auto dst = destination->GetDesc();
	const auto src = source->GetDesc();

	// Both resource must be buffers.
	if (dst.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || src.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		return { false, "CopyBufferRegion requires buffer resources." };
	}

	// Check that the copy region is within the bounds of both buffers.
	if (destinationOffset > dst.Width || byteCount > dst.Width - destinationOffset
		|| sourceOffset > src.Width || byteCount > src.Width - sourceOffset)
	{
		return { false, "CopyBufferRegion exceeds the buffer bounds." };
	}

	// Record the copy command.
	m_commandList->CopyBufferRegion(
		destination, destinationOffset,
		source, sourceOffset,
		byteCount);

	return {};
}

}
