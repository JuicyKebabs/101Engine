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

	[[nodiscard]]
	Result PrepareGeometry(
		const detail::GeometryCpuData& cpu,
		std::unique_ptr<GeometryGpuData>& gpu,
		std::unique_ptr<GeometryUploadData>& upload);

	[[nodiscard]]
	Result PrepareTexture(
		const detail::TextureCpuData& cpu,
		std::unique_ptr<TextureGpuData>& gpu,
		std::unique_ptr<TextureUploadData>& upload);

private:
	Device m_device;
	CommandQueue m_graphicsQueue;
	CommandContext m_graphicsContext;

	// Only one window output is supported for now.
	// Making them pair of arrays for multiple window outputs in the future.
	SwapChain m_windowSwapChain;
	WindowOutputHandle m_windowOutputHandle{};

	struct PendingGeometryTransfer
	{
		// Registry for this isolated initialization test only.
		detail::GeometryRegistry registry{ 0 };

		std::unique_ptr<GeometryUploadData> upload;
		std::uint64_t fenceValue = 0;
	};

	std::unique_ptr<PendingGeometryTransfer> m_pendingGeometryTransfer;

	UploadBatch m_uploadBatch;	// For resaource uploads.

private:
	// Handles failures when GPU completion cannot be confirmed
	// after command submission or presentation.
	Result HandleSubmissionFailure(Result failure);

	Result VerifyGeometryTransfer(bool indexed);

	Result VerifyGeometryFailureState();
};

}
