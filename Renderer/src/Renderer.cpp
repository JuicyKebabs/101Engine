#include "Renderer/Renderer.h"
#include "Backend/Backend.h"
#include "Backend/DepthTexture.h"
#include "Backend/ApiResult.h"
#include "Graph/FrameResourceBindings.h"
#include "Graph/GraphExecutor.h"
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
#include <cstring>

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

// A helper struct to manage the upload of resources to the GPU.
template <class Target, class Upload>
struct UploadWork
{
	Target target;
	std::unique_ptr<Upload> upload;
	bool prepared = false;
};

constexpr std::size_t MaxReadbackRequests = 32;				// Maximum number of concurrent readback requests allowed.
constexpr std::size_t MaxReadbackBytes = 128 * 1024 * 1024;	// Maximum total bytes allowed for all readback requests combined (128 MB).

// Single request for texture readback.
struct ReadbackRequest
{
	TextureReadbackDesc description;
	ReadbackState state = ReadbackState::Pending;
	Result error;
	TextureReadbackData data;								// Where the CPU result is stored after a successful readback.
	Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer;	// Where the GPU writes the readback data before it is copied to the CPU-accessible memory.
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};			// The layout of the readback buffer for the requested texture region.
	std::uint64_t transferBytes = 0;
	std::size_t reservedBytes = 0;
};

// Check if the specified region is within the bounds of the texture dimensions.
bool ContainsRegion(std::uint32_t width, std::uint32_t height, const PixelRect& region)
{
	const bool validOrigin = region.x < width && region.y < height;
	const bool nonEmpty = region.width > 0 && region.height > 0;
	const bool withinBounds = (region.x + region.width <= width) && (region.y + region.height <= height);

	return validOrigin && nonEmpty && withinBounds;
}

} // namespace

//-----------------------------------------------------------
// Private implementation details of the Renderer class.
// Declarated in cpp file for hiding from the public header.
//-----------------------------------------------------------
struct Renderer::impl
{
	//-------------------------------------------------------
	// Implementation details of the Renderer class go here.
	//-------------------------------------------------------

	backend::Backend backend;
	std::optional<detail::DrawResources> drawResources;
	std::optional<backend::DepthTexture> sceneDepth;
	std::optional<detail::SceneOutputResources> sceneOutputResources;
	uint32_t resourceOwnerId = InvalidResourceOwnerId;
	std::optional<FrameRenderData> submittedFrame;

	bool initialized = false;
	bool isRendering = false;
	FailureKind stoppedFailure = FailureKind::General;

	Result ProcessSubmittedFrame(
		FrameRenderData&& frame,
		std::vector<DrawDiagnostic>& diagnostics,
		std::size_t& drawn);

	// This confirmes that the Renderer is
	// controlable and ready for public API calls.
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

	// This checks that no frame resources are pending
	// before allowing buffer updates or destruction.
	Result CheckBufferMutation() const
	{
		Result available = CheckPublicCall();

		if (!available.IsSuccess())
		{
			return available;
		}

		if (backend.HasPendingFrameResources())
		{
			return detail::Fail("Renderer : Buffer update or destruction requires completed frame resources.");
		}

		return {};
	}

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

	//-------------------------------------------------------
	// Readback management
	//-------------------------------------------------------
	std::optional<detail::ResourceSlotTable<ReadbackHandle>> readbackSlots;
	std::vector<std::optional<ReadbackRequest>> readbacks;

	// Add readback passes to the render graph for the specified timing and passes.
	// For all readback requests, this will create a render pass 
	// that copies the texture data to a CPU-accessible buffer.
	Result AddReadbackPasses(
		ReadbackTiming timing,
		const std::vector<PlannedPass>& passes,
		graph::FrameResourceBindings& bindings,
		graph::RenderGraph& graph);

	// 
	void CompleteReadbacks();


	void FailPendingReadbacks(const Result & failure);
	Result ReleaseReadback(ReadbackHandle handle);
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
	m_pImpl->readbackSlots.emplace(ownerId);
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
	
	m_pImpl->readbacks.clear();
	m_pImpl->readbackSlots.reset();
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

	// Lambda to process failure aftertreatment.
	const auto rejectSubmission =
		[&state](Result failure)
		{
			state.FailPendingReadbacks(failure);
			return failure;
		};

	// Validate all views in the frame before accepting the submission.
	result = ValidateViewStructure(frame);

	if (!result.IsSuccess())
	{
		return rejectSubmission(std::move(result));
	}

	for (const auto& view : frame.views)
	{
		// Validate that the output for each kind of views.
		if (const auto* window = std::get_if<WindowOutputHandle>(&view.output))
		{ // Window output(back buffer)
			if (!state.backend.IsWindowOutputValid(*window))
			{
				return rejectSubmission(detail::Fail("SubmitFrameRenderData: invalid Window output."));
			}
		}
		else
		{ // Render target texture output
			const auto handle = std::get<RenderTargetTextureHandle>(view.output);

			// Try to get handle from the registry for being used.
			if (!state.drawResources->FindContent(handle))
			{
				return rejectSubmission(detail::Fail("SubmitFrameRenderData: invalid RenderTarget output."));
			}
		}
	}

	// Validate values in the FrameRenderData.
	result = detail::ValidateFrameInputs(frame);

	if (!result.IsSuccess())
	{
		return rejectSubmission(std::move(result));
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

	// Do not keep readback requests after a failed submission.
	if (hadSubmission && !result.IsSuccess())
	{
		state.FailPendingReadbacks(result);
	}

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
		const Result cleanup = state.backend.EndFrameResources();
		mergeCleanup(cleanup);

		// Rerealse any readback buffers that were used in this frame.
		if (cleanup.IsSuccess())
		{
			for (auto& request : state.readbacks)
			{
				if (request)
				{
					request->readbackBuffer.Reset();
				}
			}
		}
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

Result Renderer::impl::ProcessSubmittedFrame(FrameRenderData&& frame, std::vector<DrawDiagnostic>& diagnostics, std::size_t& drawn)
{
	//----------------------------------------------------------------------------------------------
	// 1. Cut down unusable outputs.
	//----------------------------------------------------------------------------------------------
	Result pruned = sceneOutputResources->PruneOutputs(backend, drawResources.value());

	if (!pruned.IsSuccess())
	{
		return pruned;
	}

	//----------------------------------------------------------------------------------------------
	// 2. Prepare temporal components for rendering.
	//----------------------------------------------------------------------------------------------
	graph::RenderGraph graph;											// Manager of all passes and resources for this frame.
	graph::GraphExecutor executor(graph, backend);						// Executes the compiled graph and submits commands to the GPU.
	FrameRenderer frameRenderer;										// Build the pass plan and record each pass in the graph.
	graph::FrameResourceBindings bindings(backend, graph, executor);	// Bindings for all resources used in this frame.
	detail::FrameDrawServices services;									// Services for the FrameDrawContext, including registries and backend access.
	
	services.draws = &*drawResources;					// Registry of all DrawObjects and their resources.
	services.backend = &backend;						// Access to the DX12 backend for resource creation and command submission.
	services.depth = &*sceneDepth;						//Depth texture used for depth testing and writing during rendering.
	services.bindings = &bindings;						// Pointer to the FrameResourceBindings.
	services.sceneResources = &*sceneOutputResources;	// Registry of all RenderTargetTexture resources used for output rendering.

	detail::FrameDrawContext context(frame, services, diagnostics, drawn);	// Context for selecting, uploading, and recording all draws in this frame.

	//----------------------------------------------------------------------------------------------
	// 3. Build the pass plan.
	//----------------------------------------------------------------------------------------------

	// Each View receives its own source-index list; Object values remain in current.
	// Selection occurs before Upload so shared resources can be deduplicated across Views.
	std::vector<PlannedPass> lists;
	Result selected = frameRenderer.BuildPassPlan(frame, context, lists);

	if (!selected.IsSuccess())
	{
		return selected;
	}

	//----------------------------------------------------------------------------------------------
	// 4. Gather and upload all selected resources.
	//----------------------------------------------------------------------------------------------
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

	//----------------------------------------------------------------------------------------------
	// 5. Check that all selected resources are ready for GPU use.
	//----------------------------------------------------------------------------------------------
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

	//----------------------------------------------------------------------------------------------
	// 6. Prepare for recording commands.
	//----------------------------------------------------------------------------------------------

	// All dynamic storage growth is complete. From here, Buffer/Heap addresses are stable for this frame.
	// Declare resources and Compile before recording native commands.
	Result begun = backend.BeginFrameResources();

	if (!begun.IsSuccess())
	{
		return begun;
	}

	//----------------------------------------------------------------------------------------------
	// 7. Register resource bindings for each planned pass.
	//----------------------------------------------------------------------------------------------
	
	// Add readback passes before rendering.
	Result built = AddReadbackPasses(ReadbackTiming::BeforeRendering, lists, bindings, graph);

	// Build the render passes for each planned pass in the frame.
	if (built.IsSuccess())
	{
		built = frameRenderer.BuildRenderPasses(frame, context, lists, graph);
	}

	// Add readback passes after rendering.
	if (built.IsSuccess())
	{
		built = AddReadbackPasses(ReadbackTiming::AfterRendering, lists, bindings, graph);
	}

	//----------------------------------------------------------------------------------------------
	// 8. Compile the graph (confirm all dependencies and state transitions of graph resources).
	//----------------------------------------------------------------------------------------------

	Result compiled = graph.Compile();

	if (!compiled.IsSuccess())
	{
		return compiled;
	}

	//----------------------------------------------------------------------------------------------
	// 9. Record every compiled pass and submit the command list to the GPU.
	//----------------------------------------------------------------------------------------------
	Result executed = executor.Execute();

	if (!executed.IsSuccess())
	{
		return executed;
	}

	//----------------------------------------------------------------------------------------------
	// 10. Present all Window outputs in the frame.
	//----------------------------------------------------------------------------------------------
	
	CompleteReadbacks();
	
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

Result Renderer::impl::AddReadbackPasses(
	ReadbackTiming timing,
	const std::vector<PlannedPass>& passes,
	graph::FrameResourceBindings& bindings,
	graph::RenderGraph& graph)
{
	// Iterate through all readback requests.
	for (auto& entry : readbacks)
	{
		// Only pending and matching timing requests are processed in this pass.
		if (!entry ||
			entry->state != ReadbackState::Pending ||
			entry->description.timing != timing)
		{
			continue;
		}

		auto& request = *entry;
		const auto& desc = request.description;
		auto* source = drawResources->FindContent(desc.sourceTexture);

		// Validate the target texture.
		if (!source ||
			!source->gpuData ||
			source->revision != desc.expectedSourceRevision)
		{
			request.state = ReadbackState::Failed;
			request.error = detail::Fail("Readback source texture is invalid or has changed since request.");

			continue;
		}

		const auto& gpu = *source->gpuData;

		// Check if the source texture was written to in this frame.
		const bool writtenInFrame =
			std::any_of(
				passes.begin(), passes.end(),
				[&](const PlannedPass& pass)
				{
					const auto* target = std::get_if<RenderTargetTextureHandle>(&pass.renderView.output);
					return target && *target == desc.sourceTexture && pass.colorLoad.has_value();
				});

		// Check if the requested region is valid within the source texture dimensions.
		const bool validRegion = ContainsRegion(gpu.width, gpu.height, desc.region);
		
		// Check if the timing is valid for the readback.
		// Beforerendering requires the source texture to be initialized successfully.
		// Afterrendering requires the source texture to be written to in this frame.
		const bool validTiming = timing == ReadbackTiming::BeforeRendering ? gpu.contentsValid : writtenInFrame;

		if (!validRegion || !validTiming)
		{
			request.state = ReadbackState::Failed;
			request.error = detail::Fail("Readback source texture is invalid or has changed since request.");

			continue;
		}

		auto* device = backend.GetNativeDevice();
		auto* texture = gpu.Resource();
		auto regionDesc = texture->GetDesc();
		regionDesc.Width = desc.region.width;
		regionDesc.Height = desc.region.height;

		// Calculate the footprint of the readback buffer
		// for the specified texture region.
		device->GetCopyableFootprints(
			&regionDesc,
			0,
			1,
			0,
			&request.footprint,
			nullptr,
			nullptr,
			&request.transferBytes);

		if (request.transferBytes == 0 || request.transferBytes > request.reservedBytes)
		{
			request.state = ReadbackState::Failed;
			request.error = detail::Fail("Readback source texture region is invalid or exceeds reserved buffer size.");

			continue;
		}

		// Create a readback buffer in the readback heap for the specified transfer size.
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_READBACK;

		D3D12_RESOURCE_DESC bufferDesc{};
		bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufferDesc.Width = request.transferBytes;
		bufferDesc.Height = 1;
		bufferDesc.DepthOrArraySize = 1;
		bufferDesc.MipLevels = 1;
		bufferDesc.SampleDesc.Count = 1;
		bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		const HRESULT created = device->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(request.readbackBuffer.GetAddressOf()));

		if (FAILED(created))
		{
			const HRESULT deviceStatus = device->GetDeviceRemovedReason();

			if (FAILED(deviceStatus))
			{
				Result failure = backend::MakeApiFailure("Readback buffer device removal", deviceStatus);
				failure.failureKind = FailureKind::DeviceLost;
				return failure;
			}
			
			request.state = ReadbackState::Failed;
			request.error = backend::MakeApiFailure("CreateCommittedResource(readback)", created);
			
			continue;
		}

		// Register the source texture in the render graph for readback.
		graph::GraphTextureHandle graphSource;
		Result result = bindings.ImportRenderTarget(*source, "ReadbackSource", graphSource);

		if (!result.IsSuccess())
		{
			return  result;
		}

		// Create a render pass in the graph to perform the texture readback.
		graph::RenderPassBuilder builder;
		result = builder.SetName("TextureReadback");

		if (result.IsSuccess())
		{
			result = builder.CopyTextureSource(graphSource);
		}

		if (result.IsSuccess())
		{
			// Execution lambda for the readback pass that copies the specified
			// region from the source texture to the readback buffer.
			result = builder.SetExecute(
				[this, texture, &request](graph::RenderPassContext&)->Result
				{
					// Set up the source copy location for the texture.
					D3D12_TEXTURE_COPY_LOCATION from{};
					from.pResource = texture;
					from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
					
					// Set up the destination copy location for the readback buffer.
					D3D12_TEXTURE_COPY_LOCATION to{};
					to.pResource = request.readbackBuffer.Get();
					to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
					to.PlacedFootprint = request.footprint;
					
					// Set up the source region to copy from the texture.
					const auto& region = request.description.region;
					D3D12_BOX box{};
					box.left = region.x;
					box.top = region.y;
					box.right = region.x + region.width;
					box.bottom = region.y + region.height;
					box.back = 1;
					
					// Add the copy command to the command list to copy the
					// specified region from the texture to the readback buffer.
					auto& commands = backend.GetFrameCommandContext();
					commands.GetNativeCommandList()->CopyTextureRegion(&to, 0, 0, 0, &from, &box);
					
					return {};
				});
		}

		std::optional<graph::RenderPass> pass;

		// Build the readback pass.
		if (result.IsSuccess())
		{
			result = std::move(builder).Build(pass);
		}

		// Add the readback pass to the render graph.
		if (result.IsSuccess())
		{
			result = graph.AddPass(std::move(*pass));
		}

		if (!result.IsSuccess())
		{
			return result;
		}
	}

	return {};
}

void Renderer::impl::CompleteReadbacks()
{
	for (auto& entry : readbacks)
	{
		if (!entry || entry->state != ReadbackState::Pending || !entry->readbackBuffer)
		{
			continue;
		}

		auto& request = *entry;

		// Prepare the CPU data buffer to receive the readback pixels.
		auto& data = request.data;
		data.pixels.resize(data.rowPitch * data.height);

		// Map the readback buffer to access its contents on the CPU.
		void* mapped = nullptr;
		D3D12_RANGE readRange{};
		readRange.End = static_cast<SIZE_T>(request.transferBytes);
		const HRESULT mappedResult = request.readbackBuffer->Map(0, &readRange, &mapped);

		if (FAILED(mappedResult))
		{
			data.pixels.clear();
			request.error = backend::MakeApiFailure("Readback buffer Map", mappedResult);
			request.state = ReadbackState::Failed;

			continue;
		}

		// Copy the readback pixels from the mapped buffer to the CPU data buffer.
		const auto* rows = static_cast<const std::byte*>(mapped) + request.footprint.Offset;

		for (std::size_t row = 0; row < data.height; ++row)
		{
			std::memcpy(
				data.pixels.data() + row * data.rowPitch,
				rows + row * request.footprint.Footprint.RowPitch,
				data.rowPitch);
		}

		// Unmap the readback buffer and mark the readback request as ready.
		D3D12_RANGE noWrites{};
		request.readbackBuffer->Unmap(0, &noWrites);
		request.state = ReadbackState::Ready;
	}
}

void Renderer::impl::FailPendingReadbacks(const Result& failure)
{
	// Fail all pending readback requests with the provided failure result.
	for (auto& entry : readbacks)
	{
		if (entry && entry->state == ReadbackState::Pending)
		{
			entry->state = ReadbackState::Failed;
			entry->error.succeeded = false;
			entry->error.failureKind = failure.failureKind;
		}
	}
}

Result Renderer::impl::ReleaseReadback(ReadbackHandle handle)
{
	if (readbackSlots->RequestDestroy(handle) != detail::ResourceHandleValidationResult::Success)
	{
		return detail::Fail("ReleaseReadback: invalid handle.");
	}

	readbacks[handle.slotIndex].reset();

	if (readbackSlots->ReleaseSlotAfterResourceDestruction(handle) != detail::ResourceHandleValidationResult::Success)
	{
		std::terminate(); // This should never happen; indicates a serious internal error.
	}

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

ResourceRegistrationResult<GeometryHandle> Renderer::RegisterGeometry(const GeometryDesc& desc)
{
	Result available = m_pImpl->CheckPublicCall();

	if (!available.IsSuccess())
	{
		return { std::move(available),{} };
	}

	return m_pImpl->drawResources->Register(desc);
}

ResourceRegistrationResult<TextureHandle> Renderer::RegisterTexture(const TextureDesc& desc)
{
	Result available = m_pImpl->CheckPublicCall();

	if (!available.IsSuccess())
	{
		return { std::move(available),{} };
	}

	return m_pImpl->drawResources->Register(desc);
}

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

ResourceRegistrationResult<BufferHandle> Renderer::CreateStructuredBuffer(const StructuredBufferDesc& desc)
{
	// Call create function within a ResourceCall to
	// handle exceptions and check for renderer state.
	return ResourceCall(
		[&]()
		{
			ResourceRegistrationResult<BufferHandle> created;
			created.result = m_pImpl->CheckPublicCall();

			if (!created.result.IsSuccess())
			{
				return created;
			}

			// Create the structured buffer using the backend and return the result.
			created = m_pImpl->drawResources->CreateStructuredBuffer(m_pImpl->backend, desc);
			created.result = m_pImpl->ObserveFailure(std::move(created.result));

			return created;
		}
	);
}

Result Renderer::UpdateStructuredBuffer(BufferHandle handle, std::span<const std::byte> data)
{
	// Call update function within a ResourceCall to
	// handle exceptions and check for renderer state.
	return ResourceCall(
		[&]()
		{
			Result available = m_pImpl->CheckBufferMutation();

			if (!available.IsSuccess())
			{
				return available;
			}

			Result updated = m_pImpl->drawResources->UpdateStructuredBuffer(m_pImpl->backend, handle, data);
			return m_pImpl->ObserveFailure(std::move(updated));
		});
}

Result Renderer::DestroyStructuredBuffer(BufferHandle handle)
{
	// Checking for buffer mutation to avoid destroying
	// a buffer while it is still in use by the GPU.
	Result available = m_pImpl->CheckBufferMutation();

	if (!available.IsSuccess())
	{
		return available;
	}

	return m_pImpl->drawResources->Destroy(handle);
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
		return { false, "DestroyGeometry: handle does not identify a registered geometry in this renderer initialization." };
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
		return { false, "DestroyTexture: handle does not identify a registered texture in this renderer initialization." };
	}

	return {};
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
ResourceRegistrationResult<RenderTargetTextureHandle> Renderer::CreateRenderTargetTexture(const RenderTargetTextureDesc& description)
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

ReadbackRequestResult Renderer::RequestTextureReadback(const TextureReadbackDesc& desc)
{
	return ResourceCall(
		[&]() -> ReadbackRequestResult
		{
			auto& state = *m_pImpl;
			Result available = state.CheckPublicCall();

			if (!available.IsSuccess())
			{
				return { std::move(available),{} };
			}

			// Query the target texture's metadata.
			RenderTargetTextureInfo info;
			Result queried = state.drawResources->QueryRenderTargetTextureInfo(desc.sourceTexture, info);
		
			if (!queried.IsSuccess())
			{
				return { std::move(queried),{} };
			}

			// Validate the request instructions.
			const bool validTiming =
				desc.timing == ReadbackTiming::BeforeRendering ||
				desc.timing == ReadbackTiming::AfterRendering;
			const bool matchRevision = desc.expectedSourceRevision == info.revision;
			const bool validRegion = ContainsRegion(info.width, info.height, desc.region);
		
			if (!validTiming || !matchRevision || !validRegion)
			{
				return { detail::Fail("RequestTextureReadback: invalid timing, revision, or region."),{} };
			}

			// Calculate the required memory for the readback request.
			const std::size_t bytePerPixels = info.format == TextureFormat::RGBA16_Float ? 8 : 4;
			const std::size_t rowBytes = desc.region.width * bytePerPixels;
			const std::size_t alignment = 256;
			const std::size_t paddedRowBytes = (rowBytes + alignment - 1) / alignment * alignment;
			const std::uint64_t reservationSize = static_cast<std::uint64_t>(rowBytes + paddedRowBytes) * desc.region.height;

			std::size_t requestCount = 0;
			std::size_t reservedBytes = 0;

			// Check the current readback requests to ensure we don't exceed limits.
			for (const auto& request : state.readbacks)
			{
				if (request)
				{
					requestCount++;
					reservedBytes += request->reservedBytes;
				}
			} 

			if (requestCount >= MaxReadbackRequests || reservedBytes + reservationSize > MaxReadbackBytes)
			{
				return { detail::Fail("RequestTextureReadback: exceeded maximum readback requests or bytes."),{} };
			}

			// Allocate a new readback slot and register the request.
			const auto index = state.readbackSlots->FindRegistrationSlotIndex();
			
			if (index >= state.readbacks.size())
			{
				state.readbacks.resize(index + 1);
			}

			// Register the readback request and return the handle.
			auto registration = state.readbackSlots->Register();

			if (!registration.status.IsSuccess())
			{
				return { std::move(registration.status),{} };
			}

			// Store the readback request details in the allocated slot.
			auto& request = state.readbacks[index].emplace();
			request.description = desc;
			request.reservedBytes = static_cast<std::size_t>(reservationSize);
			request.data.format = info.format;
			request.data.width = desc.region.width;
			request.data.height = desc.region.height;
			request.data.rowPitch = rowBytes;

			return { {}, registration.handle };
		});

}

ReadbackPollResult Renderer::TryTakeTextureReadback(ReadbackHandle handle, TextureReadbackData& output)
{
	auto& state = *m_pImpl;
	Result available = state.CheckPublicCall();

	if (!available.IsSuccess())
	{
		return { ReadbackState::Failed, std::move(available) };
	}

	// Validate the readback handle to ensure it is valid for new use.
	if (state.readbackSlots->ValidateForNewUse(handle) != detail::ResourceHandleValidationResult::Success)
	{
		return { ReadbackState::Failed, detail::Fail("TryTakeTextureReadback: invalid handle.") };
	}

	auto& request = *state.readbacks[handle.slotIndex];

	if (request.state == ReadbackState::Ready)
	{
		// Move the readback data to the output and release the readback slot.
		// Request is consumed and will be removed from the readback list (not reusable).
		output = std::move(request.data);
		Result  released = state.ReleaseReadback(handle);

		if (!released.IsSuccess())
		{
			return { ReadbackState::Failed, std::move(released) };
		}

		return { ReadbackState::Ready,{} };
	}

	// Provides a failure message.
	if (request.state == ReadbackState::Failed && request.error.message.empty())
	{
		Result failure = request.error;
		failure.message = "TryTakeTextureReadback: readback failed: " + failure.message;
		return { ReadbackState::Failed, std::move(failure) };
	}

	return { request.state,request.error };
}

Result Renderer::ReleaseTextureReadback(ReadbackHandle handle)
{
	auto& state = *m_pImpl;
	Result available = state.CheckPublicCall();

	if (!available.IsSuccess())
	{
		return available;
	}

	return state.ReleaseReadback(handle);
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