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

#if defined(_DEBUG)

#endif


	return {};
}

void Backend::Finalize() noexcept
{
	const auto stopWithoutCleanUp =
		[](const Result& failure)
		{
			OutputDebugStringA(failure.message.c_str());
			OutputDebugStringA("\n[101Renderer] Unsafe GPU cleanup was stopped.\n");

			// Do not unwind through GPU-owning objects.
			std::terminate();
		};

	bool deviceLost = false;

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

	m_windowOutputHandle = {};

	m_graphicsContext.Finalize();
	m_pendingGeometryTransfer.reset();

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
	if (!IsWindowOutputValid(handle))
	{
		return { false, "ClearWindowOutput: Invalid window output handle." };
	}

	ID3D12Resource* backBuffer = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv{};

	Result result = m_windowSwapChain.GetCurrentBackBuffer(backBuffer, rtv);

	if (!result.IsSuccess())
	{
		return result;
	}

	// Previous successful calls have already waited for GPU completion.
	result = m_graphicsContext.Begin();

	if (!result.IsSuccess())
	{
		return result;
	}

	result = m_graphicsContext.TransitionResource(
		backBuffer,
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	if (!result.IsSuccess())
	{
		return result;
	}

	result = m_graphicsContext.ClearRenderTarget(rtv, color);

	if (!result.IsSuccess())
	{
		return result;
	}

	result = m_graphicsContext.TransitionResource(
		backBuffer,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);

	if (!result.IsSuccess())
	{
		return result;
	}

	result = m_graphicsContext.Close();

	if (!result.IsSuccess())
	{
		return result;
	}

	const QueueSubmissionResult submission = m_graphicsQueue.Execute(m_graphicsContext);

	if (!submission.status.IsSuccess())
	{
		if (submission.executionIssued)
		{
			return HandleSubmissionFailure(submission.status);
		}

		return submission.status;
	}

	result = m_graphicsQueue.WaitForFence(submission.fenceValue);

	if (!result.IsSuccess())
	{
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

Result Backend::VerifyGeometryTransfer(bool indexed)
{
	if (m_pendingGeometryTransfer)
		return { false, "A geometry transfer is still retained." };

	try
	{
		m_pendingGeometryTransfer = std::make_unique<PendingGeometryTransfer>();
	}
	catch (const std::bad_alloc&)
	{
		return { false, "Geometry transfer allocation failed." };
	}

	auto& transfer = *m_pendingGeometryTransfer;
	const auto cancelBeforeSubmission = [&](Result failure)
		{
			m_pendingGeometryTransfer.reset();
			return failure;
		};

	const std::array<MeshVertex, 3> vertices{};
	const std::array<std::uint32_t, 3> indices{ 0, 1, 2 };
	GeometryDesc desc{};
	desc.vertices = vertices;
	if (indexed)
		desc.indices = indices;

	const auto registered = transfer.registry.Register(desc);
	if (!registered.status.IsSuccess())
		return cancelBeforeSubmission(registered.status);

	// Simulate selecting content before Destroy; this is not a Submit implementation.
	const auto* selected = transfer.registry.FindForNewUse(registered.handle);
	auto* content = transfer.registry.FindRegisteredContent(registered.handle);
	if (!content || selected != content)
		return cancelBeforeSubmission({ false, "Registered geometry content was not found." });

	assert(selected == content);
	assert(!content->gpuData);
	assert(content->state.preparationState == detail::ResourcePreparationState::Pending);
	assert(!transfer.registry.FindRegisteredContent({}));

	auto wrongOwner = registered.handle;
	++wrongOwner.ownerId;
	assert(!transfer.registry.FindRegisteredContent(wrongOwner));

	const auto destroyed = transfer.registry.RequestDestroy(registered.handle);
	if (destroyed != detail::ResourceHandleValidationResult::Success)
		return cancelBeforeSubmission({ false, "Geometry test Destroy request failed." });

	assert(!transfer.registry.FindForNewUse(registered.handle));
	assert(transfer.registry.FindRegisteredContent(registered.handle) == content);

	const auto failBeforeSubmission = [&](Result failure) -> Result
		{
			// This validation path has no outstanding earlier GPU work.
			// Device loss is not an ordinary per-resource preparation failure.
			if (failure.failureKind != FailureKind::General ||
				!m_device.CheckStatus().IsSuccess())
				return HandleSubmissionFailure(std::move(failure));

			OutputDebugStringA("[101Renderer] Geometry preparation failed: ");
			OutputDebugStringA(failure.message.c_str());
			OutputDebugStringA("\n");
			if (!transfer.registry.MarkPreparationFailed(registered.handle, *content))
				failure.message += "\nFailed state transition was rejected.";

			assert(content->state.preparationState == detail::ResourcePreparationState::Failed);
			assert(!transfer.registry.FindForNewUse(registered.handle));

			// Discard any unsubmitted commands before releasing their resources.
			// This initializer returns failure and stops; it does not reuse this context.
			m_graphicsContext.Finalize();
			m_pendingGeometryTransfer.reset();
			return failure;
		};

	Result result = PrepareGeometryUpload(
		m_device.GetNativeDevice(), content->cpuData, content->gpuData, transfer.upload);
	if (!result.IsSuccess())
		return failBeforeSubmission(std::move(result));

	const auto& gpu = *content->gpuData;
	assert(selected->gpuData.get() == &gpu);
	assert(content->state.preparationState == detail::ResourcePreparationState::Pending);
	assert(gpu.vertexCount == 3);
	assert(gpu.vertexView.SizeInBytes == sizeof(vertices));
	assert(gpu.vertexView.StrideInBytes == sizeof(MeshVertex));
	assert(gpu.indexCount == (indexed ? 3u : 0u));
	if (indexed)
	{
		assert(gpu.indexView.SizeInBytes == sizeof(indices));
		assert(gpu.indexView.Format == DXGI_FORMAT_R32_UINT);
	}
	else
	{
		assert(!gpu.indices.GetNativeResource());
		assert(!transfer.upload->indices.GetNativeResource());
		assert(gpu.indexView.BufferLocation == 0);
	}

	result = m_graphicsContext.Begin();
	if (!result.IsSuccess())
		return failBeforeSubmission(std::move(result));

	result = RecordGeometryUpload(m_graphicsContext, gpu, *transfer.upload);
	if (!result.IsSuccess())
		return failBeforeSubmission(std::move(result));

	result = m_graphicsContext.Close();
	if (!result.IsSuccess())
		return failBeforeSubmission(std::move(result));

	const auto submission = m_graphicsQueue.Execute(m_graphicsContext);
	if (!submission.status.IsSuccess())
	{
		if (submission.executionIssued)
			return HandleSubmissionFailure(submission.status);
		return failBeforeSubmission(submission.status);
	}

	transfer.fenceValue = submission.fenceValue;
	if (!submission.executionIssued ||
		!transfer.registry.MarkReadyAfterUploadSubmission(
			registered.handle, *content, transfer.fenceValue))
		return HandleSubmissionFailure({ false, "Ready transition after upload submission failed." });

	// Ready is established before CPU waiting, not by the wait.
	assert(content->state.preparationState == detail::ResourcePreparationState::Ready);
	assert(content->uploadFenceValue == transfer.fenceValue);
	assert(!transfer.registry.FindForNewUse(registered.handle)); // Destroy remains set.
	OutputDebugStringW(L"[101Renderer] Geometry Ready before CPU fence wait.\n");

	result = m_graphicsQueue.WaitForFence(transfer.fenceValue);
	if (!result.IsSuccess())
		return HandleSubmissionFailure(std::move(result));

	transfer.upload.reset();
	assert(content->state.preparationState == detail::ResourcePreparationState::Ready);
	assert(content->gpuData.get() == &gpu);
	assert(gpu.vertices.GetNativeResource());
	assert(transfer.registry.FindRegisteredContent(registered.handle) == content);
	assert(!transfer.registry.FindForNewUse(registered.handle));

	// Test only: no retained frame or later Draw uses this registry.
	// Its contents, including GPU buffers, may now be released.
	m_pendingGeometryTransfer.reset();

	OutputDebugStringW(indexed
		? L"[101Renderer] Indexed GeometryContent transfer completed.\n"
		: L"[101Renderer] Non-indexed GeometryContent transfer completed.\n");
	return {};
}

Result Backend::VerifyGeometryFailureState()
{
	// CPU-side failure test only: no commands are recorded or submitted.
	detail::GeometryRegistry registry{ 0 };
	const std::array<MeshVertex, 3> vertices{};
	GeometryDesc desc{};
	desc.vertices = vertices;

	const auto registered = registry.Register(desc);
	if (!registered.status.IsSuccess())
		return registered.status;

	auto* content = registry.FindRegisteredContent(registered.handle);
	if (!content)
		return { false, "Failure test content was not found." };

	assert(registry.FindForNewUse(registered.handle) == content);
	assert(!registry.MarkReadyAfterUploadSubmission(registered.handle, *content, 1));

	std::unique_ptr<GeometryUploadData> upload;
	const Result failure = PrepareGeometryUpload(
		nullptr, content->cpuData, content->gpuData, upload);
	if (failure.IsSuccess())
		return { false, "Null-device preparation unexpectedly succeeded." };

	OutputDebugStringA("[101Renderer] Expected preparation failure: ");
	OutputDebugStringA(failure.message.c_str());
	OutputDebugStringA("\n");

	if (!registry.MarkPreparationFailed(registered.handle, *content))
		return { false, "Failed state transition was rejected." };

	assert(content->state.preparationState == detail::ResourcePreparationState::Failed);
	assert(content->uploadFenceValue == 0);
	assert(!content->gpuData && !upload);
	assert(!registry.FindForNewUse(registered.handle));
	assert(registry.FindRegisteredContent(registered.handle) == content);
	assert(!registry.MarkReadyAfterUploadSubmission(registered.handle, *content, 1));

	// No GPU work and no retained CPU users; this local registry can now be destroyed.
	OutputDebugStringW(L"[101Renderer] Failed state and Destroy request verified.\n");
	return {};
}

}
