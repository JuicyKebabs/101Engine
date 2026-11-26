// Follow the new submission contract without weakening value-ownership checks.
// Multiple Views are supported; two Scene Views writing the same color output are still rejected.

#include "Renderer/Renderer.h"
#include "Renderer/Win32WindowOutputDesc.h"

#include <Windows.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace
{

void Require(bool condition, const char* message)
{
	if (!condition)
	{
		throw std::runtime_error(message);
	}
}

void Check(const rendering::Result& result)
{
	if (!result.IsSuccess() && result.failureKind == rendering::FailureKind::GpuCompletionUnknown)
	{
		std::cerr << result.message << '\n';
		__fastfail(FAST_FAIL_FATAL_APP_EXIT);
	}

	Require(result.IsSuccess(), result.message.c_str());
}

void CheckProcessed(const rendering::RenderResult& result)
{
	Check(result.status);
	Require(result.disposition == rendering::RenderDisposition::Processed, "Submission was not processed.");
}

void CheckSkipped(rendering::Renderer& renderer)
{
	const auto result = renderer.Render();
	Check(result.status);
	Require(result.disposition == rendering::RenderDisposition::SkippedNoSubmission, "Submission was consumed twice.");
}

void VerifyStructuredBufferLifecycle(rendering::Renderer& renderer)
{
	using namespace rendering;

	std::array<std::array<float, 4>, 3> values{};
	const auto bytes = std::as_bytes(std::span(values));
	StructuredBufferDesc desc;
	desc.elementStride = sizeof(values.front());
	Require(!renderer.CreateStructuredBuffer(desc).result.IsSuccess(), "Zero element count was accepted.");

	desc.elementCount = (std::numeric_limits<std::uint32_t>::max)();
	Require(!renderer.CreateStructuredBuffer(desc).result.IsSuccess(), "Oversized element count was accepted.");

	desc.elementCount = static_cast<std::uint32_t>(values.size());
	desc.elementStride = 6;
	Require(!renderer.CreateStructuredBuffer(desc).result.IsSuccess(), "Misaligned element stride was accepted.");

	desc.elementStride = 2052;
	Require(!renderer.CreateStructuredBuffer(desc).result.IsSuccess(), "Oversized element stride was accepted.");

	desc.elementStride = sizeof(values.front());
	const auto buffer = renderer.CreateStructuredBuffer(desc);
	Check(buffer.result);
	Require(!renderer.UpdateStructuredBuffer(buffer.handle, bytes.first(bytes.size() - 1)).IsSuccess(), "Partial update was accepted.");
	Check(renderer.UpdateStructuredBuffer(buffer.handle, bytes));
	values.front()[0] = 1.0f;
	Check(renderer.UpdateStructuredBuffer(buffer.handle, bytes));

	BufferHandle wrongOwner = buffer.handle;
	++wrongOwner.ownerId;
	Require(!renderer.UpdateStructuredBuffer(wrongOwner, bytes).IsSuccess(), "Buffer update accepted a wrong owner.");
	Require(!renderer.DestroyStructuredBuffer(wrongOwner).IsSuccess(), "Buffer destruction accepted a wrong owner.");
	Check(renderer.DestroyStructuredBuffer(buffer.handle));

	const auto replacement = renderer.CreateStructuredBuffer(desc);
	Check(replacement.result);
	Require(replacement.handle.slotIndex == buffer.handle.slotIndex, "Destroyed buffer slot was not reused.");
	Require(replacement.handle.generation != buffer.handle.generation, "Buffer generation was not advanced.");
	Require(!renderer.UpdateStructuredBuffer(buffer.handle, bytes).IsSuccess(), "Stale buffer update was accepted.");
	Require(!renderer.DestroyStructuredBuffer(buffer.handle).IsSuccess(), "Stale buffer destruction was accepted.");
	Check(renderer.UpdateStructuredBuffer(replacement.handle, bytes));
	Check(renderer.DestroyStructuredBuffer(replacement.handle));
}

void VerifyOutputPlanning(rendering::Renderer& renderer)
{
	using namespace rendering;

	RenderTargetTextureDesc description;
	description.width = 64;
	description.height = 48;
	const auto first = renderer.CreateRenderTargetTexture(description);
	const auto second = renderer.CreateRenderTargetTexture(description);
	Check(first.result);
	Check(second.result);

	FrameRenderData frame;
	RenderView scene;
	scene.id = 10;
	scene.output = first.handle;
	frame.views.push_back(scene);

	scene.id = 20;
	scene.output = second.handle;
	frame.views.push_back(scene);

	RenderView overlay;
	overlay.id = 11;
	overlay.output = first.handle;
	overlay.kind = ViewKind::Overlay;
	frame.views.push_back(overlay);

	Check(renderer.SubmitFrameRenderData(std::move(frame)));
	const auto rendered = renderer.Render();
	CheckProcessed(rendered);
	Require(rendered.drawnObjects == 2, "Clear-only Scene display passes were lost.");

	RenderTargetTextureInfo information;
	Check(renderer.GetRenderTargetTextureInfo(first.handle, information));
	Require(information.contentsValid, "Scene/Overlay output was not committed.");
	Check(renderer.GetRenderTargetTextureInfo(second.handle, information));
	Require(information.contentsValid, "Second Scene output was not committed.");

	Check(renderer.ResizeRenderTargetTexture(first.handle, 32, 24));
	Check(renderer.GetRenderTargetTextureInfo(first.handle, information));
	Require(information.revision == 2 && !information.contentsValid, "Resize did not replace the output revision.");

	Check(renderer.DestroyRenderTargetTexture(first.handle));
	Check(renderer.DestroyRenderTargetTexture(second.handle));
}

void VerifyTextureReadback(
	rendering::Renderer& renderer,
	rendering::TextureFormat format,
	const std::array<std::byte, 4>& expectedBefore,
	const std::array<std::byte, 4>& expectedAfter)
{
	using namespace rendering;

	const std::array<float, 4> red{ 1.0f, 0.0f, 0.0f, 1.0f };
	const std::array<float, 4> green{ 0.0f, 1.0f, 0.0f, 1.0f };

	RenderTargetTextureDesc targetDesc;
	targetDesc.width = 8;
	targetDesc.height = 8;
	targetDesc.format = format;
	targetDesc.optimizedClearColor = red;
	const auto target = renderer.CreateRenderTargetTexture(targetDesc);
	Check(target.result);

	const auto submitClear = [&](const std::array<float, 4>& color)
	{
		FrameRenderData frame;
		RenderView view;
		view.kind = ViewKind::Overlay;
		view.output = target.handle;
		view.clearColor = color;
		frame.views.push_back(view);
		Check(renderer.SubmitFrameRenderData(std::move(frame)));
		CheckProcessed(renderer.Render());
	};

	submitClear(red);

	RenderTargetTextureInfo info;
	Check(renderer.GetRenderTargetTextureInfo(target.handle, info));
	TextureReadbackDesc request;
	request.sourceTexture = target.handle;
	request.expectedSourceRevision = info.revision;
	request.region.x = 1;
	request.region.y = 2;
	request.region.width = (std::numeric_limits<std::uint32_t>::max)();
	request.region.height = 2;
	Require(!renderer.RequestTextureReadback(request).status.IsSuccess(), "Readback accepted a wrapping rectangle.");

	request.region.x = 0;
	request.region.width = info.width + 1;
	Require(!renderer.RequestTextureReadback(request).status.IsSuccess(), "Readback accepted a rectangle wider than the source.");

	request.region.x = 1;
	request.region.width = 3;
	request.timing = ReadbackTiming::BeforeRendering;
	const auto before = renderer.RequestTextureReadback(request);
	Check(before.status);
	const auto secondBefore = renderer.RequestTextureReadback(request);
	Check(secondBefore.status);
	request.timing = ReadbackTiming::AfterRendering;
	const auto after = renderer.RequestTextureReadback(request);
	Check(after.status);

	TextureReadbackData data;
	CheckSkipped(renderer);
	const auto pending = renderer.TryTakeTextureReadback(before.handle, data);
	Require(pending.state == ReadbackState::Pending, "A skipped frame consumed a readback.");
	submitClear(green);

	const auto checkPixels = [&](ReadbackHandle handle, const std::array<std::byte, 4>& expected)
	{
		const auto poll = renderer.TryTakeTextureReadback(handle, data);
		Check(poll.error);
		Require(poll.state == ReadbackState::Ready, "Readback did not complete.");
		Require(data.format == format, "Readback format was incorrect.");
		Require(data.width == 3 && data.height == 2, "Readback extent was incorrect.");
		Require(data.rowPitch == 12 && data.pixels.size() == 24, "Readback included row padding.");

		for (std::size_t index = 0; index < data.pixels.size(); index += 4)
		{
			const auto* pixel = data.pixels.data() + index;
			Require(std::equal(expected.begin(), expected.end(), pixel), "Readback returned the wrong pixel.");
		}
	};

	checkPixels(before.handle, expectedBefore);
	checkPixels(secondBefore.handle, expectedBefore);
	checkPixels(after.handle, expectedAfter);
	Require(!renderer.ReleaseTextureReadback(before.handle).IsSuccess(), "Ready readback handle was reusable.");

	CheckSkipped(renderer);

	const auto stale = renderer.RequestTextureReadback(request);
	Check(stale.status);
	Check(renderer.ResizeRenderTargetTexture(target.handle, 16, 16));
	submitClear(red);
	const auto failed = renderer.TryTakeTextureReadback(stale.handle, data);
	Require(failed.state == ReadbackState::Failed, "Readback used an unexpected source revision.");
	Require(!failed.error.IsSuccess() && !failed.error.message.empty(), "Readback lost its failure result.");
	Check(renderer.ReleaseTextureReadback(stale.handle));
	Check(renderer.DestroyRenderTargetTexture(target.handle));
}

void VerifyReadbackBudget(rendering::Renderer& renderer)
{
	using namespace rendering;

	RenderTargetTextureDesc targetDesc;
	targetDesc.width = 2048;
	targetDesc.height = 2048;
	const auto target = renderer.CreateRenderTargetTexture(targetDesc);
	Check(target.result);

	RenderTargetTextureInfo info;
	Check(renderer.GetRenderTargetTextureInfo(target.handle, info));
	TextureReadbackDesc request;
	request.sourceTexture = target.handle;
	request.expectedSourceRevision = info.revision;
	request.region.width = info.width;
	request.region.height = info.height;

	// Each request reserves 16 MiB for the GPU copy and 16 MiB for the CPU pixels.
	std::array<ReadbackHandle, 4> handles;

	for (auto& handle : handles)
	{
		const auto accepted = renderer.RequestTextureReadback(request);
		Check(accepted.status);
		handle = accepted.handle;
	}

	Require(!renderer.RequestTextureReadback(request).status.IsSuccess(), "Readback budget excluded CPU pixels.");

	for (const auto handle : handles)
	{
		Check(renderer.ReleaseTextureReadback(handle));
	}

	Check(renderer.DestroyRenderTargetTexture(target.handle));
}

struct TestWindow
{
	static constexpr wchar_t ClassName[] = L"101RendererFrameSubmissionTests";
	HWND window = nullptr;

	TestWindow()
	{
		WNDCLASSW windowClass{};
		windowClass.lpfnWndProc = DefWindowProcW;
		windowClass.hInstance = GetModuleHandleW(nullptr);
		windowClass.lpszClassName = ClassName;
		Require(RegisterClassW(&windowClass) != 0, "Window class registration failed.");

		// Keep the validation Window hidden; Render still exercises Clear and Present.
		window = CreateWindowExW(0, ClassName, ClassName, WS_OVERLAPPEDWINDOW, 0, 0, 320, 200, nullptr, nullptr,
                                 windowClass.hInstance, nullptr);

		if (!window)
		{
			UnregisterClassW(ClassName, windowClass.hInstance);
			throw std::runtime_error("Validation Window creation failed.");
		}
	}

	~TestWindow()
	{
		DestroyWindow(window);
		UnregisterClassW(ClassName, GetModuleHandleW(nullptr));
	}
};

void VerifyFrameSubmission()
{
	using namespace rendering;
	TestWindow window;
	Renderer renderer;
	RendererConfiguration configuration{};
	configuration.enableDebugLayer = true;
	configuration.enableGpuValidation = true;

	FrameRenderData uninitialized;
	Require(!renderer.SubmitFrameRenderData(std::move(uninitialized)).IsSuccess(),
			"Uninitialized Submit was accepted.");

	const auto uninitializedRender = renderer.Render();
	Require(!uninitializedRender.status.IsSuccess() &&
				uninitializedRender.disposition == RenderDisposition::NotProcessed,
			"Uninitialized Render was accepted.");

	Check(renderer.Initialize(configuration));
	CheckSkipped(renderer);

	VerifyStructuredBufferLifecycle(renderer);

	Win32WindowOutputDesc outputDescription{};
	outputDescription.window = window.window;
	outputDescription.width = 320;
	outputDescription.height = 200;
	const auto output = renderer.CreateWindowOutput(outputDescription);
	Check(output.status);

	FrameRenderData invalidView;
	RenderObject unavailableObject{};
	RenderView unavailableView{};
	invalidView.objects.push_back(unavailableObject);
	invalidView.views.push_back(unavailableView);
	auto* invalidStorage = invalidView.objects.data();
	Require(!renderer.SubmitFrameRenderData(std::move(invalidView)).IsSuccess(), "Invalid WindowOutput was accepted.");
	Require(invalidView.objects.data() == invalidStorage && invalidView.objects.size() == 1,
			"Rejected input was moved.");
	CheckSkipped(renderer);

	FrameRenderData duplicateSceneOutput;
	RenderView firstView;
	firstView.output = output.handle;
	firstView.id = 1;
	duplicateSceneOutput.views.push_back(firstView);

	RenderView secondView = firstView;
	secondView.id = 2;
	duplicateSceneOutput.views.push_back(secondView);

	Require(!renderer.SubmitFrameRenderData(std::move(duplicateSceneOutput)).IsSuccess(),
			"Duplicate Scene output was accepted.");
	Require(duplicateSceneOutput.views.size() == 2, "Rejected Views were moved.");

	const std::array<MeshVertex, 3> vertices{};
	GeometryDesc geometryDesc{};
	geometryDesc.vertices = vertices;
	const auto geometry = renderer.RegisterGeometry(geometryDesc);
	Check(geometry.result);

	const std::array<std::byte, 4> pixels{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{255}};

	std::array<TextureMipDesc, 1> mips{};
	mips[0].pixels = pixels;
	mips[0].rowPitch = 4;

	TextureDesc textureDescription{};
	textureDescription.width = 1;
	textureDescription.height = 1;
	textureDescription.format = TextureFormat::RGBA8_UNorm;
	textureDescription.mips = mips;

	const auto texture = renderer.RegisterTexture(textureDescription);
	Check(texture.result);

	FrameRenderData frame;
	RenderObject registeredObject{};
	registeredObject.geometry = geometry.handle;
	RenderView outputView{};
	outputView.output = output.handle;
	frame.objects.push_back(registeredObject);
	frame.objects.push_back(registeredObject);
	frame.views.push_back(outputView);
	Check(renderer.SubmitFrameRenderData(std::move(frame)));

	FrameRenderData duplicate;
	duplicate.objects.push_back(registeredObject);
	auto* duplicateStorage = duplicate.objects.data();
	Require(!renderer.SubmitFrameRenderData(std::move(duplicate)).IsSuccess(), "Duplicate Submit was accepted.");
	Require(duplicate.objects.data() == duplicateStorage && duplicate.objects.size() == 1,
			"Duplicate Submit took its input.");

	Check(renderer.DestroyGeometry(geometry.handle));
	Check(renderer.DestroyTexture(texture.handle));
	const auto destroyedDependencies = renderer.Render();
	CheckProcessed(destroyedDependencies);
	// The third actual draw is the private HDR display transform.
	Require(destroyedDependencies.drawnObjects == 3, "fallback/display draws were not recorded.");
	Require(destroyedDependencies.diagnostics.size() == 2, "Destroyed Geometry was not diagnosed per Object.");
	CheckSkipped(renderer);

	// The same now-destroyed handles are unavailable for a new submission.
	Check(renderer.SubmitFrameRenderData(std::move(duplicate)));
	CheckProcessed(renderer.Render());

	FrameRenderData empty;
	Check(renderer.SubmitFrameRenderData(std::move(empty)));
	CheckProcessed(renderer.Render());
	CheckSkipped(renderer);

	// A View-less frame also completes its uploads before returning.
	const auto secondGeometry = renderer.RegisterGeometry(geometryDesc);
	Check(secondGeometry.result);

	FrameRenderData uploadOnly;
	RenderObject uploadObject{};
	uploadObject.geometry = secondGeometry.handle;
	uploadOnly.objects.push_back(uploadObject);

	Check(renderer.SubmitFrameRenderData(std::move(uploadOnly)));
	CheckProcessed(renderer.Render());

	VerifyOutputPlanning(renderer);

	const std::array<std::byte, 4> redPixel{ std::byte{ 255 }, std::byte{ 0 }, std::byte{ 0 }, std::byte{ 255 } };
	const std::array<std::byte, 4> greenPixel{ std::byte{ 0 }, std::byte{ 255 }, std::byte{ 0 }, std::byte{ 255 } };
	VerifyTextureReadback(renderer, TextureFormat::RGBA8_UNorm, redPixel, greenPixel);

	const std::array<std::byte, 4> onePixel{ std::byte{ 1 } };
	const std::array<std::byte, 4> zeroPixel{};
	VerifyTextureReadback(renderer, TextureFormat::R32_UInt, onePixel, zeroPixel);
	VerifyReadbackBudget(renderer);

	renderer.Finalize();
	renderer.Finalize();

	Check(renderer.Initialize(configuration));
	FrameRenderData staleOutput;
	staleOutput.views.push_back(outputView);
	Require(!renderer.SubmitFrameRenderData(std::move(staleOutput)).IsSuccess(),
			"Old output survived reinitialization.");

	// Finalize must also discard an accepted, unconsumed CPU submission.
	FrameRenderData pending;
	pending.objects.push_back(uploadObject);
	Check(renderer.SubmitFrameRenderData(std::move(pending)));
	renderer.Finalize();

	Check(renderer.Initialize(configuration));
	CheckSkipped(renderer);
	renderer.Finalize();
}

} // namespace

int main()
{
	try
	{
		VerifyFrameSubmission();
		std::cout << "Frame submission checks passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
