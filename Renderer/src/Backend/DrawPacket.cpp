// R7 implementation guide: section 4.24.
// Final native validation and recording for both Geometry and procedural draws.
// Bind the required state explicitly, including scissor and removal of old VB/IB bindings.
// No resource preparation, ownership transfer or late capacity growth belongs at this point.

#include "CommandContext.h"
#include "Failure.h"

#include <cmath>

namespace rendering::backend
{

Result CommandContext::ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE depth, float value)
{
	if (!IsRecording())
	{
		return detail::Fail("ClearDepth: CommandContext is not recording.");
	}

	if (m_commandList->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		return detail::Fail("ClearDepth: a direct command list is required.");
	}

	const bool invalidValue = !std::isfinite(value) || value < 0 || value > 1;
	if (!depth.ptr || invalidValue)
	{
		return detail::Fail("ClearDepth: invalid depth Descriptor or value.");
	}

	m_commandList->ClearDepthStencilView(depth, D3D12_CLEAR_FLAG_DEPTH, value, 0, 0, nullptr);
	return {};
}

// Validate the prepared packet again at the native boundary before emitting commands.
// requiresVertexBuffer is independent of indexed mode; procedural packets have neither VB nor IB.
// Every Draw sets scissor explicitly so UI clipping cannot leak into a later Mesh.
Result CommandContext::RecordDraw(const DrawPacket& packet)
{
	if (!IsRecording())
	{
		return detail::Fail("RecordDraw: CommandContext is not recording.");
	}

	if (m_commandList->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		return detail::Fail("RecordDraw: a direct command list is required.");
	}

	const bool missingPipeline = !packet.pipeline || !packet.root || !packet.heap;
	if (missingPipeline || packet.count == 0)
	{
		return detail::Fail("RecordDraw: missing Pipeline, Root Signature, Heap or count.");
	}

	if (packet.kind != DrawKind::Vertices && packet.kind != DrawKind::Indexed)
	{
		return detail::Fail("RecordDraw: unsupported draw kind.");
	}

	if (packet.requiresVertexBuffer)
	{
		if (!packet.vertices.BufferLocation || packet.vertices.StrideInBytes == 0)
		{
			return detail::Fail("RecordDraw: the selected Shader requires a Vertex Buffer.");
		}

		UINT availableCount = packet.vertices.SizeInBytes / packet.vertices.StrideInBytes;
		if (packet.kind == DrawKind::Indexed)
		{
			const bool missingIndexBuffer = !packet.indices.BufferLocation || packet.indices.SizeInBytes == 0;
			if (missingIndexBuffer || packet.indices.Format != DXGI_FORMAT_R32_UINT)
			{
				return detail::Fail("RecordDraw: indexed drawing requires a 32-bit Index Buffer.");
			}

			availableCount = packet.indices.SizeInBytes / sizeof(std::uint32_t);
		}

		if (packet.first > availableCount || packet.count > availableCount - packet.first)
		{
			return detail::Fail("RecordDraw: range exceeds available vertices or indices.");
		}
	}
	else if (packet.kind != DrawKind::Vertices || packet.first != 0 || packet.baseVertex != 0)
	{
		return detail::Fail("RecordDraw: procedural draws require a zero-based non-indexed range.");
	}

	if (packet.scissor.right <= packet.scissor.left || packet.scissor.bottom <= packet.scissor.top)
	{
		return detail::Fail("RecordDraw: empty Scissor must be removed before recording.");
	}

	for (const auto& constant : packet.constants)
	{
		if (!constant.gpuAddress || constant.gpuAddress % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT != 0)
		{
			return detail::Fail("RecordDraw: invalid or misaligned constant address.");
		}
	}

	for (const auto& texture : packet.textures)
	{
		if (!texture.descriptor.ptr)
		{
			return detail::Fail("RecordDraw: Texture Descriptor is null.");
		}
	}

	// All validations above are CPU-side; native list recording starts here.
	// Root Signature, heap, bindings and clipping are explicit for each packet, including reset-state callbacks.
	m_commandList->SetPipelineState(packet.pipeline);
	m_commandList->SetGraphicsRootSignature(packet.root);
	m_commandList->SetDescriptorHeaps(1, &packet.heap);
	m_commandList->RSSetScissorRects(1, &packet.scissor);

	for (const auto& constant : packet.constants)
	{
		m_commandList->SetGraphicsRootConstantBufferView(constant.rootIndex, constant.gpuAddress);
	}

	for (const auto& texture : packet.textures)
	{
		m_commandList->SetGraphicsRootDescriptorTable(texture.rootIndex, texture.descriptor);
	}

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	if (packet.requiresVertexBuffer)
	{
		m_commandList->IASetVertexBuffers(0, 1, &packet.vertices);
	}
	else
	{
		// Clear the previous draw's VB binding explicitly for the SV_VertexID path.
		// The later non-indexed branch likewise removes a previous IB binding.
		D3D12_VERTEX_BUFFER_VIEW emptyVertices{};
		m_commandList->IASetVertexBuffers(0, 1, &emptyVertices);
	}

	if (packet.kind == DrawKind::Indexed)
	{
		m_commandList->IASetIndexBuffer(&packet.indices);
		m_commandList->DrawIndexedInstanced(packet.count, 1, packet.first, packet.baseVertex, 0);
	}
	else
	{
		m_commandList->IASetIndexBuffer(nullptr);
		m_commandList->DrawInstanced(packet.count, 1, packet.first, 0);
	}

	return {};
}

}
