#include "Renderer/Renderer.h"
#include "Backend/Backend.h"
#include "FrameRenderer.h"
#include "Resource/ResourceSlotTable.h"
#include "Resource/GeometryRegistry.h"
#include "Resource/TextureRegistry.h"

#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>
#include <array>
#include <cassert>
#include <algorithm>
#include <memory>
#include <new>
#include <span>
#include <stdexcept>
#include <vector>
#include <type_traits>
#include <utility>

namespace rendering
{

constexpr std::uint32_t InvalidResourceOwnerId = (std::numeric_limits<std::uint32_t>::max)();

uint32_t ReserveResourceOwnerId() noexcept
{
	static std::atomic<uint32_t> nextId{ 0 };	// This ID is shared across all Renderer instances in a DLL.

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

template<class Handle, class Content>
struct UploadTarget
{
	Handle handle;
	Content* content = nullptr;
};

using GeometryUploadTarget = UploadTarget<GeometryHandle, detail::GeometryContent>;

using TextureUploadTarget = UploadTarget<TextureHandle, detail::TextureContent>;

template<class Target, class Upload>
struct UploadWork
{
	Target target;
	std::unique_ptr<Upload> upload;
	bool prepared = false;
};

struct SubmittedObjectResources
{
	GeometryUploadTarget geometry;
	TextureUploadTarget texture;
};

struct SubmittedFrame
{
	FrameRenderData frame;
	std::vector<SubmittedObjectResources> objectResources;	// Same order as frame.objects.
};

}

struct Renderer::impl
{
	// Implementation details of the Renderer class go here.
	backend::Backend backend;
	std::optional<detail::GeometryRegistry> geometryResources;
	std::optional<detail::TextureRegistry> textureResources;
	uint32_t resourceOwnerId = InvalidResourceOwnerId;
	std::optional<SubmittedFrame> submittedFrame;

	bool initialized = false;
	bool isRendering = false;
	FailureKind stoppedFailure = FailureKind::General;

	Result CheckPublicCall() const
	{
		if (!initialized)
		{
			return { false, "Renderer is not initialized." };
		}

		if (stoppedFailure != FailureKind::General)
		{
			return { false, "Renderer is stopped after a fatal GPU failure.", stoppedFailure };
		}

		if (isRendering)
		{
			return { false, "Renderer does not allow API reentry during Render." };
		}

		return {};
	}

	Result UploadSelectedResources(std::span<const GeometryUploadTarget> geometries, std::span<const TextureUploadTarget> textures);
	
	Result CollectDestroyedResources();
};

Renderer::Renderer()
	: m_pImpl(std::make_unique<impl>())
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
		return Result{ false, "Renderer is already initialized." };
	}

	// Reserve a unique resource owner ID for this Renderer instance.
	const std::uint32_t ownerId = ReserveResourceOwnerId();

	if (ownerId == InvalidResourceOwnerId)
	{
		return { false, "Resource owner ID space is exhausted." };
	}

	// Initialize the backend with the provided configuration.
	const Result result = m_pImpl->backend.Initialize(configuration);

	if (!result.IsSuccess())
	{
		return result;
	}

	m_pImpl->geometryResources.emplace(ownerId);
	m_pImpl->textureResources.emplace(ownerId);
	m_pImpl->resourceOwnerId = ownerId;
	m_pImpl->initialized = true;

	return {};
}

void Renderer::Finalize()
{
	if (m_pImpl->isRendering)
	{
		OutputDebugStringA("[101Renderer] Finalize rejected during Render.\n");
		return;
	}

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

	m_pImpl->geometryResources.reset();
	m_pImpl->textureResources.reset();

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
		return { std::move(available), {} };
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

	if (state.submittedFrame && state.submittedFrame->frame.hasPendingSubmission)
	{
		return { false, "SubmitFrameRenderData: A submission is already pending." };
	}

	if (frame.views.size() > 1)
	{
		return { false, "SubmitFrameRenderData: Only one View is currently supported." };
	}

	for (const auto& view : frame.views)
	{
		if (!state.backend.IsWindowOutputValid(view.output))
		{
			return { false, "SubmitFrameRenderData: Invalid WindowOutput." };
		}
	}

	// Confirm what will be rendered at this timing by copying the frame and its resource references.
	// Any changes for the resources after this point will not affect the submitted frame untill the next submission.
	SubmittedFrame candidate;
	try
	{
		candidate.objectResources.reserve(frame.objects.size());

		for (const auto& object : frame.objects)
		{
			detail::GeometryContent* geometry = nullptr;
			detail::TextureContent* texture = nullptr;

			if (state.geometryResources->FindForNewUse(object.geometry))
			{
				geometry = state.geometryResources->FindRegisteredContent(object.geometry);
			}
			else
			{
				OutputDebugStringA("[101Renderer] Submit: Geometry reference is unavailable.\n");
			}

			if (state.textureResources->FindForNewUse(object.texture))
			{
				texture = state.textureResources->FindRegisteredContent(object.texture);
			}
			else if (object.texture != TextureHandle{})
			{
				OutputDebugStringA("[101Renderer] Submit: Texture reference is unavailable.\n");
			}

			candidate.objectResources.push_back({ { object.geometry, geometry }, { object.texture, texture } });
		}
	}
	catch (const std::bad_alloc&)
	{
		return { false, "SubmitFrameRenderData: Allocation failed." };
	}
	catch (const std::length_error&)
	{
		return { false, "SubmitFrameRenderData: Submission size is unsupported." };
	}

	// Nothing after this point may fail and take the caller's data without acceptance.
	static_assert(std::is_nothrow_move_assignable_v<FrameRenderData>);
	static_assert(std::is_nothrow_move_constructible_v<SubmittedFrame>);
	candidate.frame = std::move(frame);
	candidate.frame.hasPendingSubmission = true;
	state.submittedFrame.emplace(std::move(candidate));

	return {};
}

RenderResult Renderer::Render()
{
	auto& state = *m_pImpl;
	Result result = state.CheckPublicCall();

	if (!result.IsSuccess())
	{
		return { std::move(result) };
	}

	// Make sure to reset the isRendering flag by destructing 
	// this scope guard at the end of the function.
	struct RenderScope
	{
		bool& isRendering;
		~RenderScope() { isRendering = false; }
	};

	state.isRendering = true;
	RenderScope scopeGuard{ state.isRendering };

	// Process the submitted frame if it exists and has pending submission.
	const bool hadSubmission = state.submittedFrame && state.submittedFrame->frame.hasPendingSubmission;

	if (hadSubmission)
	{
		state.submittedFrame->frame.hasPendingSubmission = false;
		SubmittedFrame current = std::move(*state.submittedFrame);
		state.submittedFrame.reset();

		// Lamda to process the submitted frame and its resources.
		// This encapsulates the rendering logic.
		const auto process = [&]() -> Result
			{
				// Set of the handle and CPU data of resources which will be uploaded.
				std::vector<GeometryUploadTarget> geometryTargets;
				std::vector<TextureUploadTarget> textureTargets;

				try
				{
					// Collect the resources for the list of each kind.
					geometryTargets.reserve(current.objectResources.size());
					textureTargets.reserve(current.objectResources.size());

					for (const auto& resources : current.objectResources)
					{
						if (resources.geometry.content)
						{
							geometryTargets.push_back(resources.geometry);
						}

						if (resources.texture.content)
						{
							textureTargets.push_back(resources.texture);
						}
					}
				}
				catch (const std::bad_alloc&)
				{
					return { false, "Render: Upload target allocation failed." };
				}
				catch (const std::length_error&)
				{
					return { false, "Render: Upload target size is unsupported." };
				}

				// Upload the selected resources and prepare them for rendering.
				Result uploadedResult = state.UploadSelectedResources(geometryTargets, textureTargets);

				if (!uploadedResult.IsSuccess())
				{
					return uploadedResult;
				}

				// Get ready for beginning the frame resources.
				Result begunResult = state.backend.BeginFrameResources();

				if (!begunResult.IsSuccess())
				{
					return begunResult;
				}

				// Initial output implementation. Object drawing and Graph execution come later.
				// Current implemmentation:
				// Clear the target output and ppresent the frame for every view.
				const std::array<float, 4> clearColor{ 0.05f, 0.10f, 0.25f, 1.0f };
				
				graph::RenderGraph graph;
				graph::GraphExecutor executor(graph, state.backend);
				FrameRenderer frameRenderer;

				Result built = frameRenderer.Build(current.frame, state.backend, graph, executor);

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

				for (const auto& view : current.frame.views)
				{
					Result presented = state.backend.Present(view.output);

					if (!presented.IsSuccess())
					{
						return presented;
					}
				}

				return {};
			};

		// Call the process lambda and return its result.
		result = process();
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

	const auto disposition = !result.IsSuccess() ? RenderDisposition::NotProcessed :
		(hadSubmission ? RenderDisposition::Processed : RenderDisposition::SkippedNoSubmission);
	
	return { std::move(result), disposition };
}

GeometryRegistrationResult Renderer::RegisterGeometry(const GeometryDesc& desc)
{
	Result available = m_pImpl->CheckPublicCall();
	if (!available.IsSuccess())
	{
		return { std::move(available), {} };
	}

	return m_pImpl->geometryResources->Register(desc);
}

TextureRegistrationResult Renderer::RegisterTexture(const TextureDesc& desc)
{
	Result available = m_pImpl->CheckPublicCall();
	if (!available.IsSuccess())
	{
		return { std::move(available), {} };
	}
	return m_pImpl->textureResources->Register(desc);
}

Result Renderer::DestroyGeometry(GeometryHandle handle)
{
	Result available = m_pImpl->CheckPublicCall();
	if (!available.IsSuccess())
	{
		return available;
	}

	const auto result = m_pImpl->geometryResources->RequestDestroy(handle);

	if (result != detail::ResourceHandleValidationResult::Success)
	{
		return {
			false,
			"DestroyGeometry: handle does not identify a registered geometry "
			"in this renderer initialization."
		};
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

	const auto result = m_pImpl->textureResources->RequestDestroy(handle);

	if (result != detail::ResourceHandleValidationResult::Success)
	{
		return {
			false,
			"DestroyTexture: handle does not identify a registered texture "
			"in this renderer initialization."
		};
	}

	return {};
}

Result Renderer::impl::UploadSelectedResources(std::span<const GeometryUploadTarget> geometries, std::span<const TextureUploadTarget> textures)
{
	if (!initialized || !geometryResources || !textureResources)
	{
		return { false, "UploadSelectedResources: Renderer is not initialized." };
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
	const auto gather = [](auto targets, auto& registry, auto& work) -> Result
		{
			work.reserve(targets.size());

			for (const auto& target : targets)
			{
				// Check if registry still has the same content for the given handle.
				if (!target.content || registry.FindRegisteredContent(target.handle) != target.content)
				{
					return { false, "Upload target no longer matches its registered content." };
				}

				// Only pending content can be uploaded.
				if (target.content->state.preparationState != detail::ResourcePreparationState::Pending)
				{
					continue;
				}

				// Check duplication of the same target in the work list.
				const auto duplicate = std::find_if(work.begin(), work.end(), [&](const auto& item)
					{
						return item.target.handle == target.handle;
					});

				if (duplicate != work.end())
				{
					continue;
				}

				if (target.content->gpuData || target.content->uploadFenceValue != 0)
				{
					return { false, "Pending upload target has unexpected GPU preparation state." };
				}

				work.push_back({ target });
			}

			return {};
		};

	try
	{
		// Gather the selected geometries and textures for upload preparation.
		result = gather(geometries, *geometryResources, geometryWork);
		if (result.IsSuccess())
		{
			result = gather(textures, *textureResources, textureWork);
		}
	}
	catch (const std::bad_alloc&)
	{
		return { false, "Upload work allocation failed." };
	}
	catch (const std::length_error&)
	{
		return { false, "Upload work size is unsupported." };
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
	const auto prepare = [&](auto& work, auto& registry, auto prepareFunction) -> Result
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
						return { false, "Failed preparation state update was rejected." };
					}

					continue;
				}

				item.prepared = true;
				hasPrepared = true;
			}

			return {};
		};

	result = prepare(geometryWork, *geometryResources, &backend::Backend::PrepareGeometry);
	if (result.IsSuccess())
	{
		result = prepare(textureWork, *textureResources, &backend::Backend::PrepareTexture);
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
	// 6. Submit the upload commands and enqueue a fence signal.
	//    Mark prepared contents Ready after successful Execute + Signal.
	//    Ready does not imply CPU-observed GPU completion.
	//-------------------------------------------------------------------------------------------------	
	const auto submission = backend.SubmitUploads();
	if (!submission.status.IsSuccess())
	{
		if (submission.executionIssued)
		{
			// Submitted destinations must remain owned by their registries.
			return submission.status;
		}

		return cancelAndReset(submission.status);
	}

	if (!submission.executionIssued || submission.fenceValue == 0 || submission.fenceValue == (std::numeric_limits<std::uint64_t>::max)())
	{
		return { false, "Upload submission returned inconsistent success information.", FailureKind::GpuCompletionUnknown };
	}

	const auto markReady = [&](auto& work, auto& registry) -> Result
		{
			for (auto& item : work)
			{
				if (!item.prepared)
				{
					continue;
				}

				if (!registry.MarkReadyAfterUploadSubmission(item.target.handle, *item.target.content, submission.fenceValue))
				{
					return { false, "Ready update failed after upload submission.", FailureKind::GpuCompletionUnknown };
				}
			}

			return {};
		};

	result = markReady(geometryWork, *geometryResources);
	if (!result.IsSuccess())
	{
		return result;
	}

	return markReady(textureWork, *textureResources);
}

Result Renderer::impl::CollectDestroyedResources()
{
	if (stoppedFailure != FailureKind::General
		|| submittedFrame
		|| backend.HasPendingFrameResources())
	{
		return {};
	}

	// Collect completed uploads and release their upload buffers.
	Result result = backend.CollectUploads();

	if (!result.IsSuccess() || backend.HasPendingUploads())
	{
		return result;
	}

	// Destroy resources for the given completed fence value.
	// This does not wait for GPU completion.
	std::uint64_t completed = 0;
	result = backend.QueryCompletedGraphicsFence(completed);

	if (!result.IsSuccess())
	{
		return result;
	}

	geometryResources->CollectDestroyed(completed);
	textureResources->CollectDestroyed(completed);
	return {};
}

}
