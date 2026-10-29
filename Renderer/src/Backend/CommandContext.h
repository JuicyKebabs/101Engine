// R7 implementation guide: section 4.27.
// Native command-list operations consume validated Renderer-owned input values.
// A View establishes its viewport; each Draw establishes its scissor.
// These are different rasterizer states and must not leak across Views or Objects.

#pragma once

#include "Renderer/Result.h"
#include "DrawPacket.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <span>
#include <cstdint>

//------------------------------------------------------------------------------------
// CommandContext class
// Represents a DirectX 12 command allocator and command list for recording commands.
//------------------------------------------------------------------------------------

namespace rendering { struct Viewport; }

namespace rendering::backend
{

class CommandContext
{
public:
	CommandContext() = default;
	~CommandContext();
	
	CommandContext(const CommandContext&) = delete;
	CommandContext& operator=(const CommandContext&) = delete;
	CommandContext(CommandContext&&) = delete;
	CommandContext& operator=(CommandContext&&) = delete;

	Result Initialize(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);
	void Finalize() noexcept;

	// Destorys the previous command list and prepare for next recording.
	Result Begin();

	// Closes the command list and prepares it for execution.
	Result Close();

	// Records a transition. The caller supplies the correct resource states.
	Result TransitionResource(
		ID3D12Resource* resource,
		D3D12_RESOURCE_STATES stateBefore,
		D3D12_RESOURCE_STATES stateAfter,
		UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	// Records a set of render targets and viewport/scissor for rasterization.
	Result SetRasterOutput(
		std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> colors,
		UINT width,
		UINT height,
		D3D12_CPU_DESCRIPTOR_HANDLE depth = {});

	Result SetViewport(const Viewport& viewport, UINT outputWidth, UINT outputHeight);

	Result ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE depth, float value);

	Result RecordDraw(const DrawPacket& packet);

	// Records a clear of the entire RTV.
	// The referenced subresources must already be in RENDER_TARGET state.
	Result ClearRenderTarget(
		D3D12_CPU_DESCRIPTOR_HANDLE rtv,
		const std::array<float, 4>& clearColor);

	// Caller guarantees resource states and lifetime through GPU completion.
	Result CopyBufferRegion(
		ID3D12Resource* destination,
		std::uint64_t destinationOffset,
		ID3D12Resource* source,
		std::uint64_t sourceOffset,
		std::uint64_t byteCount);

	// Full RGBA8 2D mip from an Upload buffer. Caller supplies COPY_DEST /
	// GENERIC_READ states and retains both resources through GPU completion.
	Result CopyTextureRegion(
		ID3D12Resource* destination,
		UINT mip,
		ID3D12Resource* source,
		const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout);

	[[nodiscard]]
	bool IsClosed() const noexcept
	{
		return m_state == State::Closed;
	}

	[[nodiscard]]
	bool IsRecording() const noexcept
	{
		return m_state == State::Recording;
	}

	[[nodiscard]]
	ID3D12GraphicsCommandList* GetNativeCommandList() const noexcept
	{
		return m_commandList.Get();
	}

private:
	enum class State
	{
		Uninitialized,
		Closed,
		Recording,
		Unusable
	};

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

	State m_state = State::Uninitialized;
};

}
