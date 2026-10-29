#pragma once

#include "CommandContext.h"
#include "CommandQueue.h"
#include "GeometryGpuData.h"
#include "TextureGpuData.h"

#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rendering::backend
{

class UploadBatch
{
public:
	UploadBatch() = default;
	~UploadBatch()
	{
		// The owner must collect completed work before destruction.
		if (m_inFlight)
		{
			// In case of accidentally destroying an in-flight batch,
			// terminate the program to avoid releasing the resources while the GPU is still using them.
			std::terminate();
		}

		ResetUnsubmitted();
	}

	UploadBatch(const UploadBatch&) = delete;
	UploadBatch& operator=(const UploadBatch&) = delete;
	UploadBatch(UploadBatch&&) = delete;
	UploadBatch& operator=(UploadBatch&&) = delete;

	// Use the same device thoughout the lifetime of this object.
	Result Begin(ID3D12Device* device)
	{
		if (!device || m_recording || m_inFlight)
		{
			return { false, "UploadBatch requires a device and must not be recording or in flight." };
		}

		// Initialize command context when first usage.
		if (!m_context.GetNativeCommandList())
		{
			Result result = m_context.Initialize(device, D3D12_COMMAND_LIST_TYPE_DIRECT);

			if (!result.IsSuccess())
			{
				return result;
			}
		}

		Result result = m_context.Begin();

		if (!result.IsSuccess())
		{
			return Abort(std::move(result));
		}

		m_recording = true;
		return {};
	}

	// Submit the recorded uploads to the given queue.
	QueueSubmissionResult Submit(CommandQueue& queue)
	{
		if (!m_recording || m_inFlight)
		{
			return { { false, "UploadBatch is not recording." } };
		}

		if (m_geometryUploads.empty() && m_textureUploads.empty())
		{
			return { { false, "UploadBatch has no uploads to submit." } };
		}

		// Close the command list before submission.
		Result closed = m_context.Close();

		if (!closed.IsSuccess())
		{
			return { Abort(std::move(closed)) };
		}

		m_recording = false;

		// Execute the command list on the queue and signal the fence.
		auto submission = queue.Execute(m_context);

		if (submission.executionIssued)
		{
			m_inFlight = true;
			m_queue = &queue;
			m_fenceValue = submission.fenceValue;

			if (!submission.status.IsSuccess())
			{
				submission.status.failureKind = FailureKind::GpuCompletionUnknown;
			}
		}
		else
		{
			// Discard the recorded commands if execution was not issued.
			ResetUnsubmitted();
		}

		return submission;
	}

	// Collects the completed uploads and resets the batch for reuse.
	Result TryCollect(bool& outCollected)
	{
		outCollected = false;

		if (!m_inFlight)
		{
			return { false, "UploadBatch has no submitted work" };
		}

		if (!m_queue || m_fenceValue == 0)
		{
			return { false, "UploadBatch has no confirmed submission fence.", FailureKind::GpuCompletionUnknown };
		}

		// 
		bool completed = false;
		Result result = m_queue->QueryFenceCompletion(m_fenceValue, completed);

		if (!result.IsSuccess())
		{
			// Preserve DeviceLost; other query failures cannot prove safety.
			if (result.failureKind == FailureKind::General)
			{
				result.failureKind = FailureKind::GpuCompletionUnknown;
			}

			return result;
		}

		if (!completed)
		{
			return {};
		}

		m_geometryUploads.clear();
		m_textureUploads.clear();

		m_inFlight = false;
		m_queue = nullptr;
		m_fenceValue = 0;

		// Keep the closed Context for the next Begin.
		outCollected = true;
		return {};
	}

	[[nodiscard]]
	Result Cancel()
	{
		if (m_inFlight)
		{
			return { false, "Submitted UploadBatch cannot be canceled." };
		}

		ResetUnsubmitted();

		return {};
	}

	// Exceptional cleanup only after device removal is confirmed.
	Result ReleaseAfterDeviceLost()
	{
		auto* list = m_context.GetNativeCommandList();

		if (!list)
		{
			return { false, "UploadBatch has no device to inspect." };
		}

		Microsoft::WRL::ComPtr<ID3D12Device> device;
		const HRESULT hr = list->GetDevice(IID_PPV_ARGS(&device));

		if (FAILED(hr))
		{
			return { false, "UploadBatch could not inspect its device." };
		}

		if (SUCCEEDED(device->GetDeviceRemovedReason()))
		{
			return { false, "UploadBatch device has not been removed." };
		}

		m_inFlight = false;
		ResetUnsubmitted();

		return {};
	}

	Result AddGeometry(const GeometryGpuData& gpu, std::unique_ptr<GeometryUploadData>& upload)
	{
		return Add(gpu, upload, m_geometryUploads, RecordGeometryUpload);
	}

	Result AddTexture(const TextureGpuData& gpu, std::unique_ptr<TextureUploadData>& upload)
	{
		return Add(gpu, upload, m_textureUploads, RecordTextureUpload);
	}

	[[nodiscard]]
	bool IsRecording() const noexcept
	{
		return m_recording;
	}

	[[nodiscard]]
	bool IsInFlight() const noexcept
	{
		return m_inFlight;
	}

	[[nodiscard]]
	std::uint64_t GetFenceValue() const noexcept
	{
		return m_fenceValue;
	}

private:
	CommandContext m_context;
	
	std::vector<std::unique_ptr<GeometryUploadData>> m_geometryUploads;
	std::vector<std::unique_ptr<TextureUploadData>> m_textureUploads;

	CommandQueue* m_queue = nullptr;
	std::uint64_t m_fenceValue = 0;

	bool m_recording = false;
	bool m_inFlight = false;

private:
	template<class Gpu, class Upload, class Recorder>
	Result Add(const Gpu& gpu,
		std::unique_ptr<Upload>& upload,
		std::vector<std::unique_ptr<Upload>>& retained,
		Recorder record)
	{
		if (!m_recording || m_inFlight)
		{
			return { false, "UploadBatch is not recording or is already in flight." };
		}

		if (!upload)
		{
			return Abort({ false, "UploadBatch requires Upload data." });
		}

		// Allocate storage before recording commands.
		try
		{
			if (retained.size() >= retained.max_size())
			{
				return Abort({ false, "UploadBatch size is unsupported." });
			}

			retained.reserve(retained.size() + 1);
		}
		catch (const std::bad_alloc&)
		{
			return Abort({ false, "UploadBatch allocation failed." });
		}
		catch (const std::length_error&)
		{
			return Abort({ false, "UploadBatch size is unsupported." });
		}

		Result result = record(m_context, gpu, *upload);

		if (!result.IsSuccess())
		{
			return Abort(std::move(result));
		}

		retained.push_back(std::move(upload));
		return {};
	}

	Result Abort(Result failure)
	{
		ResetUnsubmitted();
		return failure;
	}

	void ResetUnsubmitted() noexcept
	{
		m_context.Finalize();
		m_geometryUploads.clear();
		m_textureUploads.clear();

		m_recording = false;
		m_queue = nullptr;
		m_fenceValue = 0;
	}
};

}