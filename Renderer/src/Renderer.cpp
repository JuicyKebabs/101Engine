#include "Renderer/Renderer.h"
#include "Backend/Backend.h"
#include "Backend/DepthTexture.h"
#include "FrameRenderer.h"
#include "FrameDrawContext.h"
#include "SceneOutputResources.h"
#include "Resource/ResourceSlotTable.h"
#include "Resource/GeometryRegistry.h"
#include "Resource/TextureRegistry.h"
#include "Resource/RenderTargetTextureRegistry.h"
#include "Failure.h"

#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>
#include <algorithm>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>
#include <type_traits>
#include <utility>

namespace rendering
{

constexpr std::uint32_t InvalidResourceOwnerId = (std::numeric_limits<std::uint32_t>::max)();

// Reserve a unique resource owner ID for each Renderer instance.
uint32_t ReserveResourceOwnerId() noexcept
{
	static std::atomic<uint32_t> nextId{0}; // This ID is shared across all Renderer instances in a DLL.

	uint32_t candidate = nextId.load(std::memory_order_relaxed);

	while (candidate != InvalidResourceOwnerId)
	{
		// Increment the candidate ID if it is still the current value of nextId.
		if (nextId.compare_exchange_weak(candidate, candidate + 1, std::memory_order_relaxed))
		{
			return candidate;
		}
	}

	return InvalidResourceOwnerId;
}

namespace
{

using GeometryUploadTarget = detail::GeometryUploadTarget;
using TextureUploadTarget = detail::TextureUploadTarget;

template <class Target, class Upload> struct UploadWork
{
	Target target;
	std::unique_ptr<Upload> upload;
	bool prepared = false;
};

} // namespace

//-----------------------------------------------------------
// Private implementation details of the Renderer class.
// Declarated in cpp file for hiding from the public header.
//-----------------------------------------------------------
struct Renderer::impl
{
	// Implementation details of the Renderer class go here.
	backend::Backend backend;
	std::optional<detail::DrawResources> drawResources;
	std::optional<backend::DepthTexture> sceneDepth;
	std::optional<detail::SceneOutputResources> sceneOutputResources;
	uint32_t resourceOwnerId = InvalidResourceOwnerId;
	std::optional<FrameRenderData> submittedFrame;

	bool initialized = false;
	bool isRendering = false;
	FailureKind stoppedFailure = FailureKind::General;

	Result ProcessSubmittedFrame(FrameRenderData&& frame, std::vector<DrawDiagnostic>& diagnostics, std::size_t& drawn);

	Result CheckPublicCall() const
	{
		if (!initialized)
		{
			return {false, "Renderer is not initialized."};
		}

		if (stoppedFailure != FailureKind::General)
		{
			return {false, "Renderer is stopped after a fatal GPU failure.", stoppedFailure};
		}

		if (isRendering)
		{
			return {false, "Renderer does not allow API reentry during Render."};
		}

		return {};
	}

	Result UploadSelectedResources(
		std::span<const GeometryUploadTarget> geometries,
		std::span<const TextureUploadTarget> textures);

	Result ObserveFailure(Result result)
	{
		if (!result.IsSuccess() && result.failureKind != FailureKind::General)
		{
			// Store the failure kind for later reference.
			stoppedFailure = result.failureKind;
		}

		return result;
	}

	Result CollectDestroyedResources();
};

Renderer::Renderer() : m_pImpl(std::make_unique<impl>())
{
	OutputDebugStringW(L"[101Renderer] Renderer constructed.\n");
}

Renderer::~Renderer()
{
	Finalize();
	OutputDebugStringW(L"[101Renderer] Renderer destroying.\n");
}

Result Renderer::Initialize(const RendererConfiguration& configuration)
{
	// Avoiding reinitialization
	if (m_pImpl->initialized)
	{
		return Result{false, "Renderer is already initialized."};
	}

	// Reserve a unique resource owner ID for this Renderer instance.
	const std::uint32_t ownerId = ReserveResourceOwnerId();

	if (ownerId == InvalidResourceOwnerId)
	{
		return {false, "Resource owner ID space is exhausted."};
	}

	// Initialize the backend with the provided configuration.
	const Result result = m_pImpl->backend.Initialize(configuration);

	if (!result.IsSuccess())
	{
		return result;
	}

	// Send the owner ID to each resource registry for tracking ownership.
	m_pImpl->drawResources.emplace(ownerId);
	m_pImpl->sceneDepth.emplace();
	m_pImpl->sceneOutputResources.emplace();
	m_pImpl->resourceOwnerId = ownerId;
	m_pImpl->initialized = true;

	// Do not expose a usable Renderer that cannot display missing project data.
	// All provisional resident registrations are released by Finalize on failure.
	Result residentResult = m_pImpl->drawResources->InitializeFallbacks();

	if (residentResult.IsSuccess())
	{
		// Register the fallback geometry and texture for upload to the GPU.
		GeometryUploadTarget geometryTarget;
		geometryTarget.handle = m_pImpl->drawResources->fallbackGeometry;
		geometryTarget.content = m_pImpl->drawResources->FindContent(geometryTarget.handle);

		TextureUploadTarget textureTarget;
		textureTarget.handle = m_pImpl->drawResources->fallbackTexture;
		textureTarget.content = m_pImpl->drawResources->FindContent(textureTarget.handle);

		residentResult = m_pImpl->UploadSelectedResources(std::span(&geometryTarget, 1), std::span(&textureTarget, 1));
	}

	if (residentResult.IsSuccess())
	{
		// Initialize the SceneOutputResources, which manages GPU resources for output rendering.
		residentResult = m_pImpl->sceneOutputResources->Initialize(m_pImpl->backend, *m_pImpl->drawResources);
	}

	if (!residentResult.IsSuccess())
	{
		// Finalize the Renderer when initialization fails, and return the failure result.
		residentResult = m_pImpl->ObserveFailure(std::move(residentResult));
		Finalize();
		return residentResult;
	}

	return {};
}

void Renderer::Finalize()
{
	if (m_pImpl->isRendering)
	{
		OutputDebugStringA("[101Renderer] Finalize rejected during Render.\n");
		return;
	}

	// Existing fatal policy: do not normally free a possibly executing GPU resource graph.
	// Unknown completion cannot be repaired by pretending EndFrameResources succeeded; Finalize terminates here.
	if (m_pImpl->stoppedFailure == FailureKind::GpuCompletionUnknown)
	{
		std::terminate();
	}

	if (!m_pImpl->initialized)
	{
		return;
	}

	// Discard unsubmitted uploads and confirm all submitted GPU work
	// before releasing registry-owned destination resources.
	m_pImpl->submittedFrame.reset();
	m_pImpl->backend.Finalize();

	m_pImpl->drawResources.reset();
	m_pImpl->sceneDepth.reset();
	m_pImpl->sceneOutputResources.reset();

	m_pImpl->resourceOwnerId = InvalidResourceOwnerId;
	m_pImpl->stoppedFailure = FailureKind::General;
	m_pImpl->initialized = false;

	OutputDebugStringW(L"[101Renderer] Renderer finalized.\n");
}

WindowOutputCreationResult Renderer::CreateWindowOutput(const Win32WindowOutputDesc& outputDesc)
{
	Result available = m_pImpl->CheckPublicCall();

	if (!available.IsSuccess())
	{
		return {std::move(available), {}};
	}

	// Create the window output using the backend and return the result.
	return m_pImpl->backend.CreateWindowOutput(outputDesc);
}

Result Renderer::SubmitFrameRenderData(FrameRenderData&& frame)
{
	auto& state = *m_pImpl;
	Result result = state.CheckPublicCall();

	if (!result.IsSuccess())
	{
		return result;
	}

	// Allow once to submit in a frame while Render is not executing.
	if (state.submittedFrame && state.submittedFrame->hasPendingSubmission)
	{
		return detail::Fail("SubmitFrameRenderData: a submission is already pending.");
	}

	// Validate all views in the frame before accepting the submission.
	result = ValidateViewStructure(frame);

	if (!result.IsSuccess())
	{
		return result;
	}

	for (const auto& view : frame.views)
	{
		// Validate that the output for each kind of views.
		if (const auto* window = std::get_if<WindowOutputHandle>(&view.output))
		{ // Window output(back buffer)
			if (!state.backend.IsWindowOutputValid(*window))
			{
				return detail::Fail("SubmitFrameRenderData: invalid Window output.");
			}
		}
		else
		{ // Render target texture output
			const auto handle = std::get<RenderTargetTextureHandle>(view.output);

			// Try to get handle from the registry for being used.
			if (!state.drawResources->FindContent(handle))
			{
				return detail::Fail("SubmitFrameRenderData: invalid RenderTarget output.");
			}
		}
	}

	// Validate values in the FrameRenderData.
	result = detail::ValidateFrameInputs(frame);

	if (!result.IsSuccess())
	{
		return result;
	}

	// Accept submission.
	frame.hasPendingSubmission = true;
	state.submittedFrame.emplace(std::move(frame));

	return {};
}

// One synchronous rendering window owns all temporary borrows and View lists.
// Order: select -> Upload -> readiness/capacity -> declare -> Compile -> record/submit -> complete -> Present.
// After that scope ends, EndFrameResources and registry collection may safely release completed storage.
RenderResult Renderer::Render()
{
	auto& state = *m_pImpl;
	Result result = state.CheckPublicCall();

	if (!result.IsSuccess())
	{
		RenderResult rejected;
		rejected.status = std::move(result);
		return rejected;
	}

	// RAII guard to ensure that the isRendering flag is reset
	// even if an exception occurs or the function exits early.
	struct RenderScope
	{
		bool* isRendering = nullptr;
		~RenderScope()
		{
			*isRendering = false;
		}
	};

	state.isRendering = true;
	RenderScope scopeGuard;
	scopeGuard.isRendering = &state.isRendering;

	// Process the submitted frame if it exists and has pending submission.
	std::vector<DrawDiagnostic> diagnostics;
	std::size_t drawn = 0;
	const bool hadSubmission = state.submittedFrame && state.submittedFrame->hasPendingSubmission;

	if (hadSubmission)
	{
		state.submittedFrame->hasPendingSubmission = false;

		// Move the submitted frame for consuming only in this Render() call and reset the stored submission.
		FrameRenderData current = std::move(*state.submittedFrame);
		state.submittedFrame.reset();

		// Call the process lambda and return its result.
		try
		{
			result = state.ProcessSubmittedFrame(std::move(current), diagnostics, drawn);
		}
		catch (const std::bad_alloc&)
		{
			result = detail::Fail("Render: allocation failed");
		}
		catch (const std::length_error&)
		{
			result = detail::Fail("Render: size unsupported");
		}
	}

	// Stop the renderer if a fatal failure occurred during rendering process.
	const auto stopIfFatal = [&](const Result& status)
	{
		if (!status.IsSuccess() && status.failureKind != FailureKind::General)
		{
			state.stoppedFailure = status.failureKind;
		}
	};

	stopIfFatal(result);

	// Cleanup failure is not hidden by an earlier draw error, and fatal classification is never downgraded.
	// This executes after the Graph/context/list borrows have left their process scope.
	const auto mergeCleanup = [&](Result cleanup)
	{
		stopIfFatal(cleanup);

		if (!cleanup.IsSuccess())
		{
			if (!result.IsSuccess())
			{
				cleanup.message = result.message + "\n" + cleanup.message;
			}

			result = std::move(cleanup);
		}
	};

	if (state.stoppedFailure == FailureKind::General)
	{
		mergeCleanup(state.backend.EndFrameResources());
	}

	if (state.stoppedFailure == FailureKind::General)
	{
		mergeCleanup(state.CollectDestroyedResources());
	}

	const auto disposition =
		!result.IsSuccess() ? RenderDisposition::NotProcessed
							: (hadSubmission ? RenderDisposition::Processed : RenderDisposition::SkippedNoSubmission);

	const auto completedDraws = result.IsSuccess() ? drawn : 0;
	RenderResult rendered;
	rendered.status = std::move(result);
	rendered.disposition = disposition;
	rendered.drawnObjects = completedDraws;
	rendered.diagnostics = std::move(diagnostics);
	return rendered;
}

Result Renderer::impl::ProcessSubmittedFrame(FrameRenderData&& frame, std::vector<DrawDiagnostic>& diagnostics,
                                             std::size_t& drawn)
{
	//--------------------------------------------------
	// 1. Cut down unusable outputs.
	//--------------------------------------------------
	Result pruned = sceneOutputResources->PruneOutputs(backend, drawResources.value());

	if (!pruned.IsSuccess())
	{
		return pruned;
	}

	//--------------------------------------------------
	// 2. Prepare temporal components for rendering.
	//--------------------------------------------------
	graph::RenderGraph graph;
	graph::GraphExecutor executor(graph, backend);
	FrameRenderer frameRenderer;
	graph::FrameResourceBindings bindings(backend, graph, executor);
	detail::FrameDrawServices services;
	services.draws = &*drawResources;
	services.backend = &backend;
	services.depth = &*sceneDepth;
	services.bindings = &bindings;
	services.sceneResources = &*sceneOutputResources;

	detail::FrameDrawContext context(frame, services, diagnostics, drawn);

	// Each View receives its own source-index list; Object values remain in current.
	// Selection occurs before Upload so shared resources can be deduplicated across Views.
	std::vector<PlannedPass> lists;
	Result selected = frameRenderer.BuildPassPlan(frame, context, lists);

	if (!selected.IsSuccess())
	{
		return selected;
	}

	std::vector<GeometryUploadTarget> geometries;
	std::vector<TextureUploadTarget> textures;

	if (frame.views.empty())
	{
		std::vector<PlannedDraw> uploadOnly;

		for (std::size_t index = 0; index < frame.objects.size(); ++index)
		{
			PlannedDraw draw;
			draw.objectIndex = index;
			uploadOnly.push_back(draw);
		}

		PassRecordingInputs inputs;
		selected = context.CollectUploadTargets(uploadOnly, inputs, geometries, textures);

		if (!selected.IsSuccess())
		{
			return selected;
		}
	}

	for (const PlannedPass& pass : lists)
	{
		const PassRecordingInputs inputs = MakePassRecordingInputs(frame, pass);
		selected = context.CollectUploadTargets(pass.draws, inputs, geometries, textures);

		if (!selected.IsSuccess())
		{
			return selected;
		}
	}

	// Upload completes at this boundary, including frames with no Views.
	// Later Graph failures leave no uncollected upload work.
	Result uploaded = UploadSelectedResources(geometries, textures);

	if (!uploaded.IsSuccess())
	{
		return uploaded;
	}

	std::uint64_t constantBytes = 0;
	std::uint64_t descriptorCount = 0;

	// Validate capacity before any GPU addresses are handed to commands.

	for (PlannedPass& pass : lists)
	{
		const PassRecordingInputs inputs = MakePassRecordingInputs(frame, pass);
		Result prepared = context.CheckPreparedDraws(pass.draws, inputs);

		if (!prepared.IsSuccess())
		{
			return prepared;
		}

		prepared = context.EstimateRequirements(pass.draws, inputs, constantBytes, descriptorCount);

		if (!prepared.IsSuccess())
		{
			return prepared;
		}
	}

	Result capacity = backend.EnsureFrameCapacity(static_cast<UINT>(constantBytes), static_cast<UINT>(descriptorCount));

	if (!capacity.IsSuccess())
	{
		return capacity;
	}

	// All dynamic storage growth is complete. From here, Buffer/Heap addresses are stable for this frame.
	// Declare resources and Compile before recording native commands.
	Result begun = backend.BeginFrameResources();

	if (!begun.IsSuccess())
	{
		return begun;
	}

	Result built = frameRenderer.BuildViews(frame, context, lists, graph);

	if (!built.IsSuccess())
	{
		return built;
	}

	Result compiled = graph.Compile();

	if (!compiled.IsSuccess())
	{
		return compiled;
	}

	Result executed = executor.Execute();

	if (!executed.IsSuccess())
	{
		return executed;
	}

	// Present each used Window once even when several Overlay Views target it.
	// Offscreen-only frames must not Present an omitted Window.
	std::vector<std::uint64_t> presentedWindows;

	for (const auto& view : frame.views)
	{
		const auto* window = std::get_if<WindowOutputHandle>(&view.output);

		if (!window)
		{
			continue;
		}

		const auto duplicate = std::find(presentedWindows.begin(), presentedWindows.end(), window->value);

		if (duplicate != presentedWindows.end())
		{
			continue;
		}

		Result presented = backend.Present(*window);

		if (!presented.IsSuccess())
		{
			return presented;
		}

		presentedWindows.push_back(window->value);
	}

	return {};
}

ResourceRegistrationResult<GeometryHandle> Renderer::RegisterGeometry(const GeometryDesc& desc)
{
	Result available = m_pImpl->CheckPublicCall();

	if (!available.IsSuccess())
	{
		return {std::move(available), {}};
	}

	return m_pImpl->drawResources->Register(desc);
}

ResourceRegistrationResult<TextureHandle> Renderer::RegisterTexture(const TextureDesc& desc)
{
	Result available = m_pImpl->CheckPublicCall();

	if (!available.IsSuccess())
	{
		return {std::move(available), {}};
	}

	return m_pImpl->drawResources->Register(desc);
}

Result Renderer::DestroyGeometry(GeometryHandle handle)
{
	Result available = m_pImpl->CheckPublicCall();

	if (!available.IsSuccess())
	{
		return available;
	}

	const auto result = m_pImpl->drawResources->Destroy(handle);

	if (!result.IsSuccess())
	{
		return {false, "DestroyGeometry: handle does not identify a registered geometry in this renderer initialization."};
	}

	return {};
}

Result Renderer::DestroyTexture(TextureHandle handle)
{
	Result available = m_pImpl->CheckPublicCall();

	if (!available.IsSuccess())
	{
		return available;
	}

	const auto result = m_pImpl->drawResources->Destroy(handle);

	if (!result.IsSuccess())
	{
		return {false, "DestroyTexture: handle does not identify a registered texture in this renderer initialization."};
	}

	return {};
}

Result Renderer::impl::UploadSelectedResources(
	std::span<const GeometryUploadTarget> geometries,
	std::span<const TextureUploadTarget> textures)
{
	if (!initialized || !drawResources)
	{
		return {false, "UploadSelectedResources: Renderer is not initialized."};
	}

	//-------------------------------------------------------------------------------------------------
	// 1. Query previous upload completion without waiting.
	//    Release completed upload buffers; retain the context for reuse.
	//    Destination resources remain owned by their registries.
	//-------------------------------------------------------------------------------------------------
	Result result = backend.CollectUploads();

	if (!result.IsSuccess())
	{
		return result;
	}

	using GeometryWork = UploadWork<GeometryUploadTarget, backend::GeometryUploadData>;
	using TextureWork = UploadWork<TextureUploadTarget, backend::TextureUploadData>;
	std::vector<GeometryWork> geometryWork;
	std::vector<TextureWork> textureWork;

	//-------------------------------------------------------------------------------------------------
	// 2. Gather the resources for upload.
	//    Validate the selected content without replacing it or rejecting an accepted Destroy request.
	//-------------------------------------------------------------------------------------------------
	const auto gather = [](auto targets, detail::DrawResources& registry, auto& work) -> Result
	{
		work.reserve(targets.size());

		for (const auto& target : targets)
		{
			// Check if registry still has the same content for the given handle.
			if (!target.content || !(registry.FindContent(target.handle) == target.content))
			{
				return {false, "Upload target no longer matches its registered content."};
			}

			// Only pending content can be uploaded.
			if (target.content->state.preparationState != detail::ResourcePreparationState::Pending)
			{
				continue;
			}

			// Check duplication of the same target in the work list.
			const auto duplicate = std::find_if(work.begin(), work.end(),
												[&](const auto& item) { return item.target.handle == target.handle; });

			if (duplicate != work.end())
			{
				continue;
			}

			if (target.content->gpuData)
			{
				return {false, "Pending upload target has unexpected GPU preparation state."};
			}

			typename std::remove_reference_t<decltype(work)>::value_type item;
			item.target = target;
			work.push_back(std::move(item));
		}

		return {};
	};

	try
	{
		// Gather the selected geometries and textures for upload preparation.
		result = gather(geometries, *drawResources, geometryWork);

		if (result.IsSuccess())
		{
			result = gather(textures, *drawResources, textureWork);
		}
	}
	catch (const std::bad_alloc&)
	{
		return {false, "Upload work allocation failed."};
	}
	catch (const std::length_error&)
	{
		return {false, "Upload work size is unsupported."};
	}

	if (!result.IsSuccess())
	{
		return result;
	}

	// Only before recording or after discarding this invocation's unsubmitted commands.
	const auto resetPrepared = [&]()
	{
		const auto reset = [](auto& work)
		{
			for (auto& item : work)
			{
				if (item.prepared)
				{
					item.upload.reset();
					item.target.content->gpuData.reset();
					item.prepared = false;
				}
			}
		};

		reset(geometryWork);
		reset(textureWork);
	};

	//-------------------------------------------------------------------------------------------------
	// 3. Prepare destination resources in the Default heap and upload buffers.
	//    Copy CPU data into the upload buffers.
	//    No GPU commands are recorded or submitted here.
	//-------------------------------------------------------------------------------------------------
	bool hasPrepared = false;

	const auto prepare = [&](auto& work, detail::DrawResources& registry, auto prepareFunction) -> Result
	{
		for (auto& item : work)
		{
			auto& content = *item.target.content;
			Result prepared = (backend.*prepareFunction)(content.cpuData, content.gpuData, item.upload);

			if (!prepared.IsSuccess())
			{
				if (prepared.failureKind != FailureKind::General)
				{
					return prepared;
				}

				OutputDebugStringA("[101Renderer] Resource preparation failed: ");
				OutputDebugStringA(prepared.message.c_str());
				OutputDebugStringA("\n");

				if (!registry.MarkPreparationFailed(item.target.handle, content))
				{
					return {false, "Failed preparation state update was rejected."};
				}

				continue;
			}

			item.prepared = true;
			hasPrepared = true;
		}

		return {};
	};

	result = prepare(geometryWork, *drawResources, &backend::Backend::PrepareGeometry);

	if (result.IsSuccess())
	{
		result = prepare(textureWork, *drawResources, &backend::Backend::PrepareTexture);
	}

	if (!result.IsSuccess())
	{
		resetPrepared();
		return result;
	}

	if (!hasPrepared)
	{
		return {};
	}

	//-------------------------------------------------------------------------------------------------
	// 4. Make the upload batch available and begin command recording.
	//    Wait for the previous upload batch only if it is still in flight.
	//-------------------------------------------------------------------------------------------------
	result = backend.BeginUploads();

	if (!result.IsSuccess())
	{
		resetPrepared();
		return result;
	}

	const auto cancelAndReset = [&](Result failure) -> Result
	{
		Result canceled = backend.CancelUploads();

		if (!canceled.IsSuccess())
		{
			// Preserve destinations if their commands could not be discarded.
			canceled.failureKind = FailureKind::GpuCompletionUnknown;
			return canceled;
		}

		resetPrepared();
		return failure;
	};

	//-------------------------------------------------------------------------------------------------
	// 5. Record copy commands and resource transitions.
	//    Each successful Add transfers upload ownership to UploadBatch.
	//-------------------------------------------------------------------------------------------------
	const auto record = [&](auto& work, auto addFunction) -> Result
	{
		for (auto& item : work)
		{
			if (!item.prepared)
			{
				continue;
			}

			Result added = (backend.*addFunction)(*item.target.content->gpuData, item.upload);

			if (!added.IsSuccess())
			{
				return added;
			}
		}

		return {};
	};

	result = record(geometryWork, &backend::Backend::AddGeometryUpload);

	if (result.IsSuccess())
	{
		result = record(textureWork, &backend::Backend::AddTextureUpload);
	}

	if (!result.IsSuccess())
	{
		return cancelAndReset(std::move(result));
	}

	//-------------------------------------------------------------------------------------------------
	// 6. Submit the upload commands and confirm the batch has completed.
	//    Only then publish prepared contents as Ready.
	//-------------------------------------------------------------------------------------------------
	const auto submission = backend.SubmitUploadsAndWait();

	if (!submission.status.IsSuccess())
	{
		if (submission.executionIssued)
		{
			// Submitted destinations must remain owned by their registries.
			return submission.status;
		}

		return cancelAndReset(submission.status);
	}

	if (!submission.executionIssued || submission.fenceValue == 0 ||
		submission.fenceValue == (std::numeric_limits<std::uint64_t>::max)())
	{
		return {false, "Upload submission returned inconsistent success information.",
				FailureKind::GpuCompletionUnknown};
	}

	const auto markReady = [&](auto& work, detail::DrawResources& registry) -> Result
	{
		for (auto& item : work)
		{
			if (!item.prepared)
			{
				continue;
			}

			if (!registry.MarkPrepared(item.target.handle, *item.target.content))
			{
				return {false, "Ready update failed after upload submission.", FailureKind::GpuCompletionUnknown};
			}
		}

		return {};
	};

	result = markReady(geometryWork, *drawResources);

	if (!result.IsSuccess())
	{
		return result;
	}

	return markReady(textureWork, *drawResources);
}

// Called after temporary CPU borrows have ended, or between public updates outside Render.
// A pending Submit is only a value owner and does not itself retain old Content revisions.
// Both upload and draw batches have completed before this boundary.
Result Renderer::impl::CollectDestroyedResources()
{
	if (stoppedFailure != FailureKind::General || backend.HasPendingFrameResources())
	{
		return {};
	}

	drawResources->CollectgeometryDestroyed();
	drawResources->CollectTextureDestroyed();
	return {};
}

namespace
{
// Helper function to handle resource registration and update calls with exception handling.
template <class F> auto ResourceCall(F&& function) -> decltype(function())
{
	using R = decltype(function());

	try
	{
		return function();
	}
	catch (const std::bad_alloc&)
	{
		if constexpr (std::is_same_v<R, Result>)
		{
			return {false, "Resource allocation failed"};
		}
		else
		{
			return {{false, "Resource allocation failed"}, {}};
		}
	}
	catch (const std::length_error&)
	{
		if constexpr (std::is_same_v<R, Result>)
		{
			return {false, "Resource size unsupported"};
		}
		else
		{
			return {{false, "Resource size unsupported"}, {}};
		}
	}
}
} // namespace

ResourceRegistrationResult<ShaderHandle> Renderer::RegisterShader(const ShaderDesc& desc)
{
	return ResourceCall(
		[&]() -> ResourceRegistrationResult<ShaderHandle>
		{
			auto result = m_pImpl->CheckPublicCall();

			if (!result.IsSuccess())
			{
				return {result, {}};
			}

			return m_pImpl->drawResources->Register(desc);
		});
}

ResourceRegistrationResult<PipelineHandle> Renderer::RegisterPipeline(const PipelineDesc& desc)
{
	return ResourceCall(
		[&]() -> ResourceRegistrationResult<PipelineHandle>
		{
			auto result = m_pImpl->CheckPublicCall();

			if (!result.IsSuccess())
			{
				return {result, {}};
			}

			return m_pImpl->drawResources->Register(desc);
		});
}

ResourceRegistrationResult<MaterialHandle> Renderer::RegisterMaterial(const MaterialDesc& desc)
{
	return ResourceCall(
		[&]() -> ResourceRegistrationResult<MaterialHandle>
		{
			auto result = m_pImpl->CheckPublicCall();

			if (!result.IsSuccess())
			{
				return {result, {}};
			}

			return m_pImpl->drawResources->Register(desc);
		});
}

// Resource updates run outside Render, after the previous GPU work has completed.
Result Renderer::UpdateMaterial(MaterialHandle handle, const MaterialDesc& desc)
{
	return ResourceCall(
		[&]()
		{
			auto result = m_pImpl->CheckPublicCall();

			if (!result.IsSuccess())
			{
				return result;
			}

			return m_pImpl->drawResources->UpdateMaterial(handle, desc);
		});
}

// The Registry copies the caller's CPU arrays before returning.
// GPU preparation is deferred; its failure will diagnose/skip the accepted revision rather than use old
// pixels/vertices.
Result Renderer::UpdateGeometry(GeometryHandle handle, const GeometryDesc& desc)
{
	return ResourceCall(
		[&]()
		{
			auto result = m_pImpl->CheckPublicCall();

			if (!result.IsSuccess())
			{
				return result;
			}

			return m_pImpl->drawResources->Update(handle, desc);
		});
}

// Copy CPU pixels now, prepare GPU storage later through the existing Upload path.
// This remains the ordinary Texture API; render-target pixels are never updated through this call.
Result Renderer::UpdateTexture(TextureHandle handle, const TextureDesc& desc)
{
	return ResourceCall(
		[&]()
		{
			auto result = m_pImpl->CheckPublicCall();

			if (!result.IsSuccess())
			{
				return result;
			}

			return m_pImpl->drawResources->Update(handle, desc);
		});
}

Result Renderer::GetGeometryData(GeometryHandle handle, GeometryData& output) const
{
	Result available = m_pImpl->CheckPublicCall();

	if (!available.IsSuccess())
	{
		return available;
	}

	return m_pImpl->drawResources->GetGeometryData(handle, output);
}

Result Renderer::GetShaderMetadata(ShaderHandle handle, ShaderMetadata& output) const
{
	return ResourceCall(
		[&]() -> Result
		{
			auto result = m_pImpl->CheckPublicCall();

			if (!result.IsSuccess())
			{
				return result;
			}

			const auto* shader = m_pImpl->drawResources->FindContent(handle);

			if (!shader)
			{
				return {false, "Shader metadata target unavailable"};
			}

			auto copy = shader->metadata;
			output = std::move(copy);

			return {};
		});
}

Result Renderer::DestroyShader(ShaderHandle handle)
{
	return ResourceCall(
		[&]()
		{
			auto result = m_pImpl->CheckPublicCall();

			if (!result.IsSuccess())
			{
				return result;
			}

			return m_pImpl->drawResources->Destroy(handle);
		});
}

Result Renderer::DestroyPipeline(PipelineHandle handle)
{
	return ResourceCall(
		[&]()
		{
			auto result = m_pImpl->CheckPublicCall();

			if (!result.IsSuccess())
			{
				return result;
			}

			return m_pImpl->drawResources->Destroy(handle);
		});
}

Result Renderer::DestroyMaterial(MaterialHandle handle)
{
	return ResourceCall(
		[&]()
		{
			auto result = m_pImpl->CheckPublicCall();

			if (!result.IsSuccess())
			{
				return result;
			}

			return m_pImpl->drawResources->Destroy(handle);
		});
}

// Create a GPU-generated output in its dedicated owner.
// A handle is returned only for a complete color/descriptor/Depth set; contents are initially invalid.
ResourceRegistrationResult<RenderTargetTextureHandle> Renderer::CreateRenderTargetTexture(
	const RenderTargetTextureDesc& description)
{
	return ResourceCall(
		[&]()
		{
			ResourceRegistrationResult<RenderTargetTextureHandle> created;
			created.result = m_pImpl->CheckPublicCall();

			if (!created.result.IsSuccess())
			{
				return created;
			}

			created = m_pImpl->drawResources->CreateRenderTargetTexture(m_pImpl->backend.GetNativeDevice(), description);
			created.result = m_pImpl->ObserveFailure(std::move(created.result));
			return created;
		});
}

// Wait for known idle outside Render before changing the physical output.
// Registry Resize keeps the logical handle and commits a new complete revision only on success.
// Material handles stay valid, but cached Info/projection values are the caller's responsibility.
Result Renderer::ResizeRenderTargetTexture(RenderTargetTextureHandle handle, std::uint32_t width, std::uint32_t height)
{
	return ResourceCall(
		[&]()
		{
			Result result = m_pImpl->CheckPublicCall();

			if (!result.IsSuccess())
			{
				return result;
			}

			if (!m_pImpl->drawResources->FindContent(handle))
			{
				return detail::Fail("ResizeRenderTargetTexture: invalid output.");
			}

			if (width == 0 || height == 0)
			{
				return detail::Fail("ResizeRenderTargetTexture: zero dimensions are invalid.");
			}

			result = m_pImpl->ObserveFailure(m_pImpl->backend.WaitIdle());

			if (!result.IsSuccess())
			{
				return result;
			}

			result = m_pImpl->drawResources->ResizeRenderTargetTexture(m_pImpl->backend.GetNativeDevice(), handle, width, height);
			return m_pImpl->ObserveFailure(std::move(result));
		});
}

// Stop future resolution through this handle immediately.
// The owner still retains physical contents until the submitted write/read Fence is complete.
Result Renderer::DestroyRenderTargetTexture(RenderTargetTextureHandle handle)
{
	Result result = m_pImpl->CheckPublicCall();

	if (!result.IsSuccess())
	{
		return result;
	}

	return m_pImpl->drawResources->Destroy(handle);
}

// Return metadata by value without exposing borrowed native descriptors or owner pointers.
// A failed query preserves the caller's output variable.
Result Renderer::GetRenderTargetTextureInfo(RenderTargetTextureHandle handle, RenderTargetTextureInfo& output) const
{
	Result result = m_pImpl->CheckPublicCall();

	if (!result.IsSuccess())
	{
		return result;
	}

	return m_pImpl->drawResources->QueryRenderTargetTextureInfo(handle, output);
}

// Window Resize has a different failure guarantee from ordinary image replacement.
// Only after successful BackBuffer replacement discard the old Window Depth, which is already idle.
// A partial DXGI failure leaves readiness paused for a positive-size retry with the same handle.
Result Renderer::ResizeWindowOutput(WindowOutputHandle handle, std::uint32_t width, std::uint32_t height)
{
	return ResourceCall(
		[&]()
		{
			Result result = m_pImpl->CheckPublicCall();

			if (!result.IsSuccess())
			{
				return result;
			}

			result = m_pImpl->ObserveFailure(m_pImpl->backend.ResizeWindowOutput(handle, width, height));

			if (!result.IsSuccess())
			{
				return result;
			}

			// WaitIdle succeeded; no Graph borrows the old Window Depth.
			m_pImpl->sceneDepth.emplace();
			return result;
		});
}

} // namespace rendering
