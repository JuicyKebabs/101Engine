#include <Windows.h>
#include <array>
#include <cmath>
#include <string>
#include <intrin.h>
#include <cassert>
#include <cstddef>
#include <utility>

#include "Renderer/Renderer.h"
#include "Renderer/Math.h"
#include "Renderer/Win32WindowOutputDesc.h"

//-------------------------------------------------
// Entry point for the RendererSandbox application
//-------------------------------------------------

namespace
{

constexpr wchar_t WindowClassName[] = L"101RendererSandboxWindow";

bool VerifyRendererMath()
{
	// Call out-of-line functions through the Renderer DLL.
	const rendering::Vector3 v{3.0f, 4.0f, 0.0f};
	const float length = v.Length();
	const auto translation =
		rendering::Matrix4x4::CreateTranslation({2.0f, 3.0f, 4.0f});
	const auto transformed =
		translation.TransformPoint({1.0f, 2.0f, 3.0f});

	const bool succeeded =
		std::abs(length - 5.0f) < 1e-6f &&
		transformed.NearEqual({3.0f, 5.0f, 7.0f});

	const std::string message =
		std::string("[RendererSandbox] Math check ") +
		(succeeded ? "passed" : "FAILED") +
		": length=" + std::to_string(length) +
		", point=(" + std::to_string(transformed.x) +
		", " + std::to_string(transformed.y) +
		", " + std::to_string(transformed.z) + ")\n";
	OutputDebugStringA(message.c_str());
	return succeeded;
}

void ShowWin32Error(const wchar_t* message)
{
	const DWORD errorCode = GetLastError();

	const std::wstring errorMessage = 
		std::wstring(message) + L" failed.\nError code: " + std::to_wstring(errorCode);

	MessageBoxW(
		nullptr,
		errorMessage.c_str(),
		L"RendererSandbox Error",
		MB_OK | MB_ICONERROR);
}

std::wstring Utf8ToWide(const std::string& text)
{
	const int required = MultiByteToWideChar(
		CP_UTF8, MB_ERR_INVALID_CHARS,
		text.c_str(), -1,
		nullptr, 0);

	if (required == 0)
	{
		return L"Failed to convert the renderer diagnostic.";
	}

	std::wstring converted(
		static_cast<std::size_t>(required), L'\0');

	const int written = MultiByteToWideChar(
		CP_UTF8, MB_ERR_INVALID_CHARS,
		text.c_str(), -1,
		converted.data(), required);

	if (written == 0)
	{
		return L"Failed to convert the renderer diagnostic.";
	}

	// The conversion includes the terminating null character.
	converted.pop_back();
	return converted;
}

void ShowRendererFailure(HWND window, const rendering::Result& result)
{
	if (result.failureKind == rendering::FailureKind::GpuCompletionUnknown)
	{
		OutputDebugStringA(result.message.c_str());
		OutputDebugStringW(
			L"\n[RendererSandbox] Fatal GPU synchronization failure.\n");

		// Do not unwind through GPU-owning destructors.
		__fastfail(FAST_FAIL_FATAL_APP_EXIT);
	}

	const std::wstring message = Utf8ToWide(result.message);

	OutputDebugStringW(message.c_str());
	OutputDebugStringW(L"\n");

	MessageBoxW(
		window,
		message.c_str(),
		L"RendererSandbox Error",
		MB_OK | MB_ICONERROR);
}

LRESULT CALLBACK WindowProcedure(
	HWND window,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT paint{};
		BeginPaint(window, &paint);
		EndPaint(window, &paint);
		return 0;
	}
	}

	return DefWindowProcW(window, message, wParam, lParam);
}

}

int WINAPI wWinMain(
	HINSTANCE instance,
	HINSTANCE,
	PWSTR,
	int showCommand)
{
	if (!VerifyRendererMath())
	{
		return 1;
	}

	// Rigister the window class and its window procedure
	WNDCLASSEXW windowClass{};
	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProcedure;
	windowClass.hInstance = instance;
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.lpszClassName = WindowClassName;

	if (RegisterClassExW(&windowClass) == 0)
	{
		ShowWin32Error(L"RegisterClassEx");
		return 1;
	}

	// Create a registerd window
	const HWND window = CreateWindowExW(
		0,
		WindowClassName,
		L"RendererSandbox",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		1280,
		720,
		nullptr,
		nullptr,
		instance,
		nullptr);

	if (window == nullptr)
	{
		ShowWin32Error(L"CreateWindowExW");
		UnregisterClassW(WindowClassName, instance);
		return 1;
	}

	// Try to get the client rect of the window,
	RECT clientRect{};
	if (!GetClientRect(window, &clientRect))
	{
		ShowWin32Error(L"GetClientRect");

		DestroyWindow(window);
		UnregisterClassW(WindowClassName, instance);
		return 1;
	}

	// Calculate the client area size and check if it is valid.
	const LONG clientWidth = clientRect.right - clientRect.left;
	const LONG clientHeight = clientRect.bottom - clientRect.top;

	// Client area must be bigger than zero,
	if (clientWidth <= 0 || clientHeight <= 0)
	{
		MessageBoxW(
			window,
			L"The initial client area must have a positive size.",
			L"RendererSandbox Error",
			MB_OK | MB_ICONERROR);

		DestroyWindow(window);
		UnregisterClassW(WindowClassName, instance);
		return 1;
	}

	// Initialize the renderer

	// Send the window information to the renderer to create the swapchain.
	const rendering::Win32WindowOutputDesc windowOutputDesc
	{
		window,
		static_cast<uint32_t>(clientWidth),
		static_cast<uint32_t>(clientHeight)
	};

	int exitCode = 0;

	{
		rendering::Renderer renderer;

		// Prepare configuration for the renderer initialization
		rendering::RendererConfiguration configuration{};
		configuration.enableDebugLayer = true;
		configuration.enableGpuValidation = true;

		// Initialize the renderer
		rendering::Result result = renderer.Initialize(configuration);

		// Create the window output for the renderer to render to the created window.
		rendering::WindowOutputHandle windowOutput{};

		if (result.IsSuccess())
		{
			const rendering::WindowOutputCreationResult creation = renderer.CreateWindowOutput(windowOutputDesc);

			result = creation.status;

			if (result.IsSuccess())
			{
				windowOutput = creation.handle;

				const std::string message =
					"[RendererSandbox] Window output created. Handle: "
					+ std::to_string(windowOutput.value)
					+ "\n";

				OutputDebugStringA(message.c_str());
			}
		}

		rendering::GeometryHandle geometryHandle;
		rendering::TextureHandle textureHandle;

		if (result.IsSuccess())
		{
			std::array<rendering::MeshVertex, 3> vertices{};
			vertices[0].position = { -0.5f, -0.5f, 0.0f };
			vertices[1].position = { 0.0f, 0.5f, 0.0f };
			vertices[2].position = { 0.5f, -0.5f, 0.0f };
			rendering::GeometryDesc geometryDesc{};
			geometryDesc.vertices = vertices;

			const auto geometry = renderer.RegisterGeometry(geometryDesc);
			result = geometry.status;
			geometryHandle = geometry.handle;
		}

		if (result.IsSuccess())
		{
			const std::array<std::byte, 4> pixels{ std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255} };
			const std::array<rendering::TextureMipDesc, 1> mips{ { { pixels, 4 } } };
			rendering::TextureDesc textureDesc{};
			textureDesc.width = 1;
			textureDesc.height = 1;
			textureDesc.mips = mips;

			const auto texture = renderer.RegisterTexture(textureDesc);
			result = texture.status;
			textureHandle = texture.handle;
		}

		if (!result.IsSuccess())
		{
			ShowRendererFailure(window, result);
			exitCode = 1;
		}
		else
		{
			ShowWindow(window, showCommand);

			MSG message{};
			bool firstFramePresented = false;

			for (;;)
			{
				if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
				{
					if (message.message == WM_QUIT)
					{
						exitCode = static_cast<int>(message.wParam);
						break;
					}

					TranslateMessage(&message);
					DispatchMessageW(&message);
					continue;
				}

				if (IsIconic(window))
				{
					if (!WaitMessage())
					{
						ShowWin32Error(L"WaitMessage");
						exitCode = 1;
						break;
					}

					continue;
				}

				// Objects currently exercise registration and upload; Render displays a clear color.
				rendering::FrameRenderData frame;
				frame.objects.push_back({ geometryHandle, textureHandle });
				frame.views.push_back({ windowOutput });
				result = renderer.SubmitFrameRenderData(std::move(frame));

				rendering::RenderResult rendered;
				if (result.IsSuccess())
				{
					rendered = renderer.Render();
					result = std::move(rendered.status);
				}

				if (!result.IsSuccess())
				{
					ShowRendererFailure(window, result);
					exitCode = 1;
					break;
				}

				if (!firstFramePresented && rendered.disposition == rendering::RenderDisposition::Processed)
				{
					OutputDebugStringA("[RendererSandbox] First frame presented.\n");
					firstFramePresented = true;
				}
			}
		}

		renderer.Finalize();
	}

	// Destroy the window and unregister the window class before exiting the application.
	if (IsWindow(window))
	{
		DestroyWindow(window);
	}

	UnregisterClassW(WindowClassName, instance);

	return exitCode;
}
