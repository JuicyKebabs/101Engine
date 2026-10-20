#include "Backend.h"
#include "UploadBatch.h"

#include <array>
#include <cassert>
#include <atomic>
#include <limits>
#include <utility>
#include <exception>

namespace rendering::backend
{

WindowOutputHandle ReserveWindowOutputHandle() noexcept
{
	// Counter for generating unique window output handles.
	// This is shared across all instances of the Backend class within the same DLL.
	static std::atomic<uint64_t> nextId{ 1 };

	uint64_t candidate = nextId.load(std::memory_order_relaxed);

	for (;;)
	{
		if (candidate == std::numeric_limits<uint64_t>::max())
		{
			return {};
		}

		// Comparing the next id with the candidate and incrementind next id if they are equal.
		if (nextId.compare_exchange_weak(candidate, candidate + 1, std::memory_order_relaxed))
		{
			return { candidate };
		}

		// Loop comparing and incrementing until a unique ID is reserved.
	}
}

Result Backend::Initialize(const RendererConfiguration& configuration)
{
	// Initialize device
	const Result deviceResult = m_device.Initialize(configuration);

	if (!deviceResult.IsSuccess())
	{
		return deviceResult;
	}

	// Initialize command queue
	const Result commandQueueResult = m_graphicsQueue.Initialize(
		m_device.GetNativeDevice(),
		D3D12_COMMAND_LIST_TYPE_DIRECT);

	if (!commandQueueResult.IsSuccess())
	{
		m_device.Finalize();
		return commandQueueResult;
	}

	// Initialize command context
	const Result commandContextResult = m_graphicsContext.Initialize(
		m_device.GetNativeDevice(),
		D3D12_COMMAND_LIST_TYPE_DIRECT);

	if (!commandContextResult.IsSuccess())
	{
		Finalize();
		return commandContextResult;
	}

	// Initialize frame resources
	const Result frameResult = m_frameResources.Initialize(
		&m_device,
		1024 * 1024,	// 1 MB for constants
		1024,			// 1024 descriptors
		4096);			// Retain up to 4096 objects

	if (!frameResult.IsSuccess())
	{
		Finalize();
		return frameResult;
	}

	return {};
}

void Backend::Finalize() noexcept
{
	// Lamda to terminates the application with debug output
	const auto stopWithoutCleanUp =
		[](const Result& failure)
		{
			OutputDebugStringA(failure.message.c_str());
			OutputDebugStringA("\n[101Renderer] Unsafe GPU cleanup was stopped.\n");

			// Do not unwind through GPU-owning objects.
			std::terminate();
		};

	bool deviceLost = false;

	// Check the status of the device before releasing resources.
	if (m_device.IsInitialized())
	{
		const Result status = m_device.CheckStatus();

		if (!status.IsSuccess())
		{
			// Terminate the application when device is not lost despite the failure.
			if (status.failureKind != FailureKind::DeviceLost)
			{
				stopWithoutCleanUp(status);
			}

			deviceLost = true;
		}
	}

	// Discard unsubmitted commands before releasing their destinations.
	if (!m_uploadBatch.IsInFlight())
	{
		const Result canceled = m_uploadBatch.Cancel();

		if (!canceled.IsSuccess())
		{
			stopWithoutCleanUp(canceled);
		}
	}

	// Finish the graphics queue to ensure all submitted commands are completed before releasing resources.
	if (!deviceLost && m_graphicsQueue.IsInitialized())
	{
		const Result completed = m_graphicsQueue.Flush();

		if (!completed.IsSuccess())
		{
			const Result status = m_device.CheckStatus();

			if (!status.IsSuccess() && status.failureKind == FailureKind::DeviceLost)
			{
				deviceLost = true;
			}
			else
			{
				stopWithoutCleanUp(completed);
			}
		}
	}

	// Make sure to finish the upload batch after the GPU completion check.
	if (m_uploadBatch.IsInFlight())
	{
		if (deviceLost)
		{
			const Result released = m_uploadBatch.ReleaseAfterDeviceLost();

			if (!released.IsSuccess())
			{
				stopWithoutCleanUp(released);
			}
		}
		else
		{
			bool collected = false;
			const Result result = m_uploadBatch.TryCollect(collected);

			if (!result.IsSuccess())
			{
				stopWithoutCleanUp(result);
			}

			if (!collected)
			{
				stopWithoutCleanUp({ false, "Finalize: Upload completion was not confirmed." });
			}
		}
	}

	const Result canceled = m_uploadBatch.Cancel();

	if (!canceled.IsSuccess())
	{
		stopWithoutCleanUp(canceled);
	}


	// Releases the components
	m_windowOutputHandle = {};

	m_graphicsContext.Finalize();
	m_frameResources.Finalize();
	m_frameCommandsNeedDiscard = false;
	m_frameActive = false;
	m_frameCompletionUnknown = false;
	m_frameFence = 0;

	m_windowSwapChain.Finalize();
	m_graphicsQueue.Finalize();
	m_device.Finalize();
}

WindowOutputCreationResult Backend::CreateWindowOutput(const Win32WindowOutputDesc& outputDesc)
{
	if (!m_device.IsInitialized() || !m_graphicsQueue.IsInitialized())
	{
		return { { false, "Backend is not initialized." },{} };
	}

	if (m_windowSwapChain.IsInitialized())
	{
		return { { false, "Only one window output is currently supported." },{} };
	}

	// Reserve a unique handle for the new window output.
	const WindowOutputHandle handle = ReserveWindowOutputHandle();

	if (handle.value == 0)
	{
		return { { false, "Window output IDs are exhausted." },{} };
	}

	// Initialize the swap chain for the given window output description.
	Result result = m_windowSwapChain.Initialize(
		m_device.GetNativeDevice(),
		m_device.GetNativeFactory(),
		m_graphicsQueue.GetNativeCommandQueue(),
		outputDesc);

	if (!result.IsSuccess())
	{
		// The handle is not used if the swap chain initialization fails.
		return { std::move(result), {} };
	}

	m_windowOutputHandle = handle;

	return { {}, handle };
}

Result Backend::ClearWindowOutput(WindowOutputHandle handle, const std::array<float, 4>& color)
{
	if (!m_frameActive || !IsWindowOutputValid(handle))
	{
		return { false, "ClearWindowOutput requires an active frame and valid output." };
	}

	ID3D12Resource* backBuffer = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};

	Result result = m_windowSwapChain.GetCurrentBackBuffer(backBuffer, rtvHandle);

	if (!result.IsSuccess())
	{
		return result;
	}

	// Begin recording commands for the current frame.
	result = BeginFrameCommands();

	if (!result.IsSuccess())
	{
		return result;
	}

	// Transition RTV state from being presented to being written to as a render target.
	result = m_graphicsContext.TransitionResource(
		backBuffer,
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	if (!result.IsSuccess())
	{
		return result;
	}

	// Add command to clear the back buffer with the specified color.
	result = m_graphicsContext.ClearRenderTarget(rtvHandle, color);

	if (!result.IsSuccess())
	{
		return result;
	}

	// Transition RTV state back to being presented after clearing.
	result = m_graphicsContext.TransitionResource(
		backBuffer,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);

	if (!result.IsSuccess())
	{
		return result;
	}

	// Closing the command list after recording all commands for the current frame.
	result = m_graphicsContext.Close();

	if (!result.IsSuccess())
	{
		return result;
	}

	// Execute the recorded commands. (Sends them to the GPU)
	const QueueSubmissionResult submission = SubmitFrameCommands();
	if (!submission.status.IsSuccess())
	{
		return submission.status;
	}

	// Wait for GPU completion of the submitted commands.
	result = m_graphicsQueue.WaitForFence(submission.fenceValue);

	if (!result.IsSuccess())
	{
		// Deal with the device lost or unknown GPU completion situation.
		return HandleSubmissionFailure(std::move(result));
	}

	return {};
}

Result Backend::Present(WindowOutputHandle handle)
{
	if (!IsWindowOutputValid(handle))
	{
		return { false, "Present: Invalid window output handle." };
	}

	Result presentResult = m_windowSwapChain.Present(1, 0);

	if (!presentResult.IsSuccess())
	{
		const Result deviceStatus = m_device.CheckStatus();

		if (!deviceStatus.IsSuccess() && deviceStatus.failureKind == FailureKind::DeviceLost)
		{
			presentResult.failureKind = FailureKind::DeviceLost;
			presentResult.message += "\n";
			presentResult.message += deviceStatus.message;

			Finalize();
			return presentResult;
		}
	}

	// Confirm completion after the presentation call.
	// Even a failed Present does not justify skipping synchronization.
	Result completionResult = m_graphicsQueue.Flush();

	if (!completionResult.IsSuccess())
	{
		if (!presentResult.IsSuccess())
		{
			completionResult.message =
				presentResult.message + "\n" + completionResult.message;
		}

		return HandleSubmissionFailure(std::move(completionResult));
	}

	return presentResult;
}

bool Backend::IsWindowOutputValid(WindowOutputHandle handle) const noexcept
{
	return handle.value != 0
		&& handle.value == m_windowOutputHandle.value
		&& m_windowSwapChain.IsInitialized();
}

Result Backend::CollectUploads()
{
	if (!m_uploadBatch.IsInFlight())
	{
		return {};
	}

	bool collected = false;
	Result result = m_uploadBatch.TryCollect(collected);

	if (!result.IsSuccess())
	{
		return HandleSubmissionFailure(std::move(result));
	}

	return {};
}

Result Backend::BeginUploads()
{
	if (!m_device.IsInitialized() || !m_graphicsQueue.IsInitialized())
	{
		return { false, "BeginUploads: Backend is not initialized." };
	}

	if (m_uploadBatch.IsRecording())
	{
		return { false, "BeginUploads: Upload recording has already begun." };
	}

	// Collect any completed uploads before starting a new batch.
	Result result = CollectUploads();

	if (!result.IsSuccess())
	{
		return result;
	}

	if (m_uploadBatch.IsInFlight())
	{
		result = m_graphicsQueue.WaitForFence(m_uploadBatch.GetFenceValue());

		if (!result.IsSuccess())
		{
			return HandleSubmissionFailure(std::move(result));
		}

		result = CollectUploads();

		if (!result.IsSuccess())
		{
			return result;
		}

		if (m_uploadBatch.IsInFlight())
		{
			return HandleSubmissionFailure({ false, "BeginUploads: Batch remained in flight after waiting."});
		}
	}

	return m_uploadBatch.Begin(m_device.GetNativeDevice());
}

QueueSubmissionResult Backend::SubmitUploads()
{
	auto submission = m_uploadBatch.Submit(m_graphicsQueue);

	if (!submission.status.IsSuccess())
	{
		if (submission.executionIssued || submission.status.failureKind != FailureKind::General)
		{
			submission.status = HandleSubmissionFailure(submission.status);
		}
	}

	return submission;
}

Result Backend::PrepareGeometry(
	const detail::GeometryCpuData& cpu,
	std::unique_ptr<GeometryGpuData>& gpu,
	std::unique_ptr<GeometryUploadData>& upload)
{
	if (!m_device.IsInitialized() || !m_graphicsQueue.IsInitialized())
	{
		return { false, "PrepareGeometry: Backend is not initialized." };
	}

	Result result = PrepareGeometryUpload(m_device.GetNativeDevice(), cpu, gpu, upload);

	if (!result.IsSuccess())
	{
		if (result.failureKind != FailureKind::General || !m_device.CheckStatus().IsSuccess())
		{
			return HandleSubmissionFailure(std::move(result));
		}
	}

	return result;
}

Result Backend::PrepareTexture(
	const detail::TextureCpuData& cpu,
	std::unique_ptr<TextureGpuData>& gpu,
	std::unique_ptr<TextureUploadData>& upload)
{
	if (!m_device.IsInitialized() || !m_graphicsQueue.IsInitialized())
	{
		return { false, "PrepareTexture: Backend is not initialized." };
	}

	Result result = PrepareTextureUpload(m_device.GetNativeDevice(), cpu, gpu, upload);

	if (!result.IsSuccess())
	{
		if (result.failureKind != FailureKind::General || !m_device.CheckStatus().IsSuccess())
		{
			return HandleSubmissionFailure(std::move(result));
		}
	}

	return result;
}

Result Backend::HandleSubmissionFailure(Result failure)
{
	const Result deviceStatus = m_device.CheckStatus();

	if (!deviceStatus.IsSuccess() && deviceStatus.failureKind == FailureKind::DeviceLost)
	{// In case of the device lost.
		failure.failureKind = FailureKind::DeviceLost;
		failure.message += "\n";
		failure.message += deviceStatus.message;

		// Finalize the backend to clean up resources without waiting for GPU completion.
		Finalize();
		return failure;
	}

	// It is impossible to confirm the GPU completion.
	failure.failureKind = FailureKind::GpuCompletionUnknown;
	failure.message += "\nGPU completion could not be confirmed.";

	// Return the failure result without finalizing the backend to avoid potential crashes.
	// User application must handle this situation by the abnormal termination of the application.
	// It's also required to skip the destructor of the backend.
	return failure;
}

Result Backend::BeginFrameResources()
{
	if (!m_device.IsInitialized() || !m_graphicsQueue.IsInitialized())
	{
		return { false, "BeginFrameResources: Backend is not initialized." };
	}

	if (m_frameActive)
	{
		return { false, "BeginFrameResources: Frame resources are already active." };
	}

	// Graphics context must be initialized before starting frame resources.
	if (!m_graphicsContext.GetNativeCommandList())
	{
		Result initialized = m_graphicsContext.Initialize(
			m_device.GetNativeDevice(),
			D3D12_COMMAND_LIST_TYPE_DIRECT);

		if (!initialized.IsSuccess())
		{
			return initialized;
		}
	}

	m_frameActive = true;
	m_frameFence = 0;	// Begin with no fence value.
	m_frameCompletionUnknown = false;
	
	return {};
}

Result Backend::BeginFrameCommands()
{
	if (!m_frameActive || m_frameCompletionUnknown || m_frameCommandsNeedDiscard)
	{
		return { false, "Frame command recording is unavailable." };
	}

	// Wait for the previous frame's GPU completion.
	if (m_frameFence != 0)
	{
		Result completion = m_graphicsQueue.WaitForFence(m_frameFence);
		
		if (!completion.IsSuccess())
		{
			m_frameCompletionUnknown = true;
			return HandleSubmissionFailure(std::move(completion));
		}
	}
	m_frameCommandsNeedDiscard = true;

	// Begin recording commands for the current frame.
	return m_graphicsContext.Begin();
}

QueueSubmissionResult Backend::SubmitFrameCommands()
{
	if (!m_frameActive || m_frameCompletionUnknown || !m_frameCommandsNeedDiscard)
	{
		return { { false, "Frame submission is unavailable." } };
	}

	// Execute the recorded commands for the current frame and signal the fence
	auto submission = m_graphicsQueue.Execute(m_graphicsContext);

	// Check execution completion
	if (submission.executionIssued)
	{
		m_frameCommandsNeedDiscard = false;

		// Check signal completion and fence value validity
		if (!submission.status.IsSuccess() || submission.fenceValue == 0 ||
			submission.fenceValue == (std::numeric_limits<std::uint64_t>::max)())
		{
			m_frameCompletionUnknown = true;

			if (submission.status.IsSuccess())
			{
				submission.status = { false, "Frame submission returned no valid fence." };
			}

			submission.status = HandleSubmissionFailure(std::move(submission.status));

			return submission;
		}

		m_frameFence = submission.fenceValue;
	}

	return submission;
}

Result Backend::EndFrameResources()
{
	if (!m_frameActive)
	{
		return {};
	}

	if (m_frameCompletionUnknown)
	{
		return { false, "Frame completion is unknown; retain all storage.", FailureKind::GpuCompletionUnknown };
	}

	// Wait for the GPU to complete the frame's commands.
	if (m_frameFence != 0)
	{
		Result completed = m_graphicsQueue.WaitForFence(m_frameFence);

		if (!completed.IsSuccess())
		{
			m_frameCompletionUnknown = true;
			return HandleSubmissionFailure(std::move(completed));
		}
	}

	const bool recreateContext = m_frameCommandsNeedDiscard;

	if (recreateContext)
	{
		m_graphicsContext.Finalize();
	}

	m_frameCommandsNeedDiscard = false;
	m_frameResources.ResetAfterCompletion();
	m_frameActive = false;
	m_frameFence = 0;

	if (recreateContext)
	{
		Result initialized = m_graphicsContext.Initialize(
			m_device.GetNativeDevice(),
			D3D12_COMMAND_LIST_TYPE_DIRECT);

		if (!initialized.IsSuccess() && !m_device.CheckStatus().IsSuccess())
		{
			return HandleSubmissionFailure(std::move(initialized));
		}

		return initialized;
	}

	return {};
}

Result Backend::QueryCompletedGraphicsFence(std::uint64_t& outValue)
{
	Result result = m_graphicsQueue.QueryCompletedFenceValue(outValue);

	// Do not finish without checking the failuer reason.
	if (!result.IsSuccess())
	{
		return HandleSubmissionFailure(std::move(result));
	}

	return {};
}

Result Backend::AllocateConstants(std::span<const std::byte> data, ConstantAllocation& output)
{
	if (!m_frameActive || m_frameCompletionUnknown)
	{
		return { false, "Constant allocation requires an active healthy frame." };
	}

	return m_frameResources.AllocateConstants(data, output);
}

Result Backend::CopyFrameDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE source, D3D12_GPU_DESCRIPTOR_HANDLE& output)
{
	if (!m_frameActive || m_frameCompletionUnknown)
	{
		return { false, "Descriptor allocation requires an active healthy frame." };
	}

	return m_frameResources.CopyDescriptor(source, output);
}

Result Backend::KeepFrameObjectAlive(IUnknown* object)
{
	if (!m_frameActive || m_frameCompletionUnknown)
	{
		return { false, "Object retention requires an active healthy frame." };
	}

	return m_frameResources.KeepAlive(object);
}

}
