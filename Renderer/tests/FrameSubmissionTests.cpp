#include "Renderer/Renderer.h"
#include "Renderer/Win32WindowOutputDesc.h"

#include <Windows.h>
#include <intrin.h>
#include <array>
#include <cstddef>
#include <iostream>
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
		window = CreateWindowExW(0, ClassName, ClassName, WS_OVERLAPPEDWINDOW, 0, 0, 320, 200, nullptr, nullptr, windowClass.hInstance, nullptr);
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
	FrameRenderData uninitialized;
	Require(!renderer.SubmitFrameRenderData(std::move(uninitialized)).IsSuccess(), "Uninitialized Submit was accepted.");
	const auto uninitializedRender = renderer.Render();
	Require(!uninitializedRender.status.IsSuccess() && uninitializedRender.disposition == RenderDisposition::NotProcessed, "Uninitialized Render was accepted.");

	Check(renderer.Initialize({ true, true }));
	CheckSkipped(renderer);

	const auto output = renderer.CreateWindowOutput({ window.window, 320, 200 });
	Check(output.status);

	FrameRenderData invalidView;
	invalidView.objects.push_back({});
	invalidView.views.push_back({});
	auto* invalidStorage = invalidView.objects.data();
	Require(!renderer.SubmitFrameRenderData(std::move(invalidView)).IsSuccess(), "Invalid WindowOutput was accepted.");
	Require(invalidView.objects.data() == invalidStorage && invalidView.objects.size() == 1, "Rejected input was moved.");
	CheckSkipped(renderer);

	FrameRenderData tooManyViews;
	tooManyViews.views.push_back({ output.handle });
	tooManyViews.views.push_back({ output.handle });
	Require(!renderer.SubmitFrameRenderData(std::move(tooManyViews)).IsSuccess(), "Unsupported multiple Views were accepted.");
	Require(tooManyViews.views.size() == 2, "Rejected Views were moved.");

	const std::array<MeshVertex, 3> vertices{};
	GeometryDesc geometryDesc{};
	geometryDesc.vertices = vertices;
	const auto geometry = renderer.RegisterGeometry(geometryDesc);
	Check(geometry.status);

	const std::array<std::byte, 4> pixels{ std::byte{1}, std::byte{2}, std::byte{3}, std::byte{255} };
	const std::array<TextureMipDesc, 1> mips{ { { pixels, 4 } } };
	const auto texture = renderer.RegisterTexture({ 1, 1, TextureFormat::RGBA8_UNorm, mips });
	Check(texture.status);

	FrameRenderData frame;
	frame.objects.push_back({ geometry.handle, texture.handle });
	frame.objects.push_back({ geometry.handle, texture.handle });
	frame.views.push_back({ output.handle });
	Check(renderer.SubmitFrameRenderData(std::move(frame)));

	FrameRenderData duplicate;
	duplicate.objects.push_back({ geometry.handle, texture.handle });
	auto* duplicateStorage = duplicate.objects.data();
	Require(!renderer.SubmitFrameRenderData(std::move(duplicate)).IsSuccess(), "Duplicate Submit was accepted.");
	Require(duplicate.objects.data() == duplicateStorage && duplicate.objects.size() == 1, "Duplicate Submit took its input.");

	Check(renderer.DestroyGeometry(geometry.handle));
	Check(renderer.DestroyTexture(texture.handle));
	CheckProcessed(renderer.Render());
	CheckSkipped(renderer);

	// The same now-destroyed handles are unavailable for a new submission.
	Check(renderer.SubmitFrameRenderData(std::move(duplicate)));
	CheckProcessed(renderer.Render());

	FrameRenderData empty;
	Check(renderer.SubmitFrameRenderData(std::move(empty)));
	CheckProcessed(renderer.Render());
	CheckSkipped(renderer);

	// A View-less frame prepares resources and can be finalized with uploads retained.
	const auto secondGeometry = renderer.RegisterGeometry(geometryDesc);
	Check(secondGeometry.status);
	FrameRenderData uploadOnly;
	uploadOnly.objects.push_back({ secondGeometry.handle, {} });
	Check(renderer.SubmitFrameRenderData(std::move(uploadOnly)));
	CheckProcessed(renderer.Render());
	renderer.Finalize();
	renderer.Finalize();

	Check(renderer.Initialize({ true, true }));
	FrameRenderData staleOutput;
	staleOutput.views.push_back({ output.handle });
	Require(!renderer.SubmitFrameRenderData(std::move(staleOutput)).IsSuccess(), "Old output survived reinitialization.");

	// Finalize must also discard an accepted, unconsumed CPU submission.
	FrameRenderData pending;
	pending.objects.push_back({ secondGeometry.handle, {} });
	Check(renderer.SubmitFrameRenderData(std::move(pending)));
	renderer.Finalize();

	Check(renderer.Initialize({ true, true }));
	CheckSkipped(renderer);
	renderer.Finalize();
}

}

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
