#pragma once

#include "Renderer/RendererConfiguration.h"
#include "Renderer/WindowOutput.h"

#include "Device.h"
#include "CommandQueue.h"
#include "CommandContext.h"
#include "SwapChain.h"

#include "GeometryGpuData.h"
#include "Resource/GeometryRegistry.h"
#include "UploadBatch.h"
#include "FrameResources.h"

#include <array>
#include <memory>
#include <cstdint>

//-----------------------------------------------------------------------------------------
// Backend class
// Represents the rendering backend, which manages the DirectX 12 resources and operations.
// Whole DirectX 12 functions are encapsulated in this class.
//-----------------------------------------------------------------------------------------

namespace rendering::backend
{

class Backend
{
public:
	Result Initialize(const RendererConfiguration& configuration);
	void Finalize() noexcept;

	// Create the swapchain and reserve the handle for the given Win32 window output description.
	WindowOutputCreationResult CreateWindowOutput(const Win32WindowOutputDesc& outputDesc);

	Result ClearWindowOutput(
		WindowOutputHandle handle,
		const std::array<float, 4>& color);

	Result Present(WindowOutputHandle handle);

	Result BeginFrameResources();
	Result EndFrameResources();

	Result QueryCompletedGraphicsFence(std::uint64_t& outValue);
	[[nodiscard]] bool HasPendingFrameResources() const noexcept
	{
		return m_frameActive;
	}

	Result AllocateConstants(std::span<const std::byte> data, ConstantAllocation& output);
	Result CopyFrameDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE source, D3D12_GPU_DESCRIPTOR_HANDLE& output);
	Result KeepFrameObjectAlive(IUnknown* object);

	[[nodiscard]] ID3D12DescriptorHeap* GetFrameDescriptorHeap() const noexcept
	{
		return m_frameResources.GetDescriptorHeap();
	}
	
	// Check if there is a valid window output for the given handle.
	[[nodiscard]]
	bool IsWindowOutputValid(WindowOutputHandle handle) const noexcept;

	// Resource upload related functions. Only one upload batch is supported for now.
	[[nodiscard]] Result CollectUploads();
	[[nodiscard]] Result BeginUploads();
	[[nodiscard]] QueueSubmissionResult SubmitUploads();
	[[nodiscard]] Result CancelUploads()
	{
		return m_uploadBatch.Cancel();
	}
	[[nodiscard]] bool HasPendingUploads() const noexcept
	{
		return m_uploadBatch.IsRecording() || m_uploadBatch.IsInFlight();
	}

	[[nodiscard]] Result AddGeometryUpload(const GeometryGpuData& gpu, std::unique_ptr<GeometryUploadData>& upload)
	{
		return m_uploadBatch.AddGeometry(gpu, upload);
	}
	[[nodiscard]] Result AddTextureUpload(const TextureGpuData& gpu, std::unique_ptr<TextureUploadData>& upload)
	{
		return m_uploadBatch.AddTexture(gpu, upload);
	}

	[[nodiscard]] Result PrepareGeometry(
		const detail::GeometryCpuData& cpu,
		std::unique_ptr<GeometryGpuData>& gpu,
		std::unique_ptr<GeometryUploadData>& upload);

	[[nodiscard]] Result PrepareTexture(
		const detail::TextureCpuData& cpu,
		std::unique_ptr<TextureGpuData>& gpu,
		std::unique_ptr<TextureUploadData>& upload);

private:
	Device m_device;
	CommandQueue m_graphicsQueue;
	CommandContext m_graphicsContext;

	FrameResources m_frameResources;
	bool m_frameActive = false;
	bool m_frameCommandsNeedDiscard = false;
	bool m_frameCompletionUnknown = false;
	std::uint64_t m_frameFence = 0;

	// Only one window output is supported for now.
	// Making them pair of arrays for multiple window outputs in the future.
	SwapChain m_windowSwapChain;
	WindowOutputHandle m_windowOutputHandle{};

	UploadBatch m_uploadBatch;	// For resaource uploads.

private:
	// Handles failures when GPU completion cannot be confirmed
	// after command submission or presentation.
	Result HandleSubmissionFailure(Result failure);

	[[nodiscard]] Result BeginFrameCommands();
	[[nodiscard]] QueueSubmissionResult SubmitFrameCommands();
};

}
