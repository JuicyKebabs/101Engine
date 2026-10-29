#include <Windows.h>
#include <array>
#include <cmath>
#include <string>
#include <intrin.h>
#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>
#include <chrono>

#include "ShaderFile.h"

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

	// Create a registered window
	const HWND window = CreateWindowExW(
		0,
		WindowClassName,
		L"R4: M shared color / O material override / P pipeline override",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		1920,
		1080,
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
		rendering::MaterialHandle materialHandle;
		rendering::MaterialHandle alternateMaterial;
		rendering::PipelineHandle extraPipeline;

		if (result.IsSuccess())
		{
			// Build an indexed cube with independent normals and UVs for each face.
			std::vector<rendering::MeshVertex> vertices;
			std::vector<std::uint32_t> indices;

			const rendering::Vector3 normals[] =
			{
				{ 0, 0, -1 },
				{ 0, 0, 1 },
				{ -1, 0, 0 },
				{ 1, 0, 0 },
				{ 0, 1, 0 },
				{ 0, -1, 0 }
			};

			const rendering::Vector3 corners[][4] =
			{
			    {{-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {1, -1, -1}},
			    {{1, -1, 1}, {1, 1, 1}, {-1, 1, 1}, {-1, -1, 1}},
			    {{-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1}, {-1, -1, -1}},
			    {{1, -1, -1}, {1, 1, -1}, {1, 1, 1}, {1, -1, 1}},
			    {{-1, 1, -1}, {-1, 1, 1}, {1, 1, 1}, {1, 1, -1}},
			    {{-1, -1, 1}, {-1, -1, -1}, {1, -1, -1}, {1, -1, 1}}
			};

			for (std::uint32_t face = 0; face < 6; ++face)
			{
				const rendering::Vector2 uv[] = {{0, 1}, {0, 0}, {1, 0}, {1, 1}};

				for (std::uint32_t v = 0; v < 4; ++v)
				{
					rendering::MeshVertex vertex;
					vertex.position = corners[face][v] * 0.35f;
					vertex.normal = normals[face];
					vertex.uv = uv[v];
					vertices.push_back(vertex);
				}

				for (auto i : {0u, 1u, 2u, 0u, 2u, 3u})
				{
					indices.push_back(face * 4 + i);
				}
			}

			rendering::GeometryDesc geometryDesc{ vertices, indices };

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

		if (result.IsSuccess())
		{
			try
			{
				const auto vsBytes = sandbox::ReadShader("MeshVS.cso");
				const auto psBytes = sandbox::ReadShader("LitPS.cso");
				const auto extraBytes = sandbox::ReadShader("ExtraPS.cso");

				auto vs = renderer.RegisterShader({rendering::ShaderStage::Vertex, vsBytes});
				auto ps = renderer.RegisterShader({rendering::ShaderStage::Pixel, psBytes});
				auto extra = renderer.RegisterShader({rendering::ShaderStage::Pixel, extraBytes});

				result = vs.result;

				if (result.IsSuccess())
				{
					result = ps.result;
				}

				if (result.IsSuccess())
				{
					result = extra.result;
				}

				if (result.IsSuccess())
				{
					rendering::PipelineDesc pipelineDesc{ vs.handle, ps.handle };
					pipelineDesc.cull = rendering::CullMode::None;

					// Register the regular and override pipelines.
					const auto pipeline = renderer.RegisterPipeline(pipelineDesc);
					result = pipeline.result;

					pipelineDesc.pixelShader = extra.handle;
					const auto overridePipeline = renderer.RegisterPipeline(pipelineDesc);
					if (result.IsSuccess())
					{
						result = overridePipeline.result;
					}

					extraPipeline = overridePipeline.handle;

					if (result.IsSuccess())
					{
						rendering::MaterialDesc material;
						material.pipeline = pipeline.handle;
						material.parameters =
						{
							{ "BaseColor", rendering::Vector4{ 1, 0.3f, 0.15f, 1 } },
							{ "BaseTexture", textureHandle },
							{ "ExtraTexture", textureHandle }
						};

						auto registered = renderer.RegisterMaterial(material);
						result = registered.result;
						materialHandle = registered.handle;

						// Keep a second material for the per-view override.
						material.parameters[0].value = rendering::Vector4{ 0.15f, 0.5f, 1, 1 };
						auto other = renderer.RegisterMaterial(material);
						if (result.IsSuccess())
						{
							result = other.result;
						}

						alternateMaterial = other.handle;
					}
				}
			}
			catch (const std::exception& e)
			{
				result = {false, e.what()};
			}
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
			bool useMaterialOverride = false;
			bool usePipelineOverride = false;

			const auto startTime = std::chrono::steady_clock::now();

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

				const float seconds =
					std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();

				if (GetAsyncKeyState('O') & 1)
				{
					useMaterialOverride = !useMaterialOverride;
				}

				if (GetAsyncKeyState('P') & 1)
				{
					usePipelineOverride = !usePipelineOverride;
				}

				if (GetAsyncKeyState('M') & 1)
				{
					rendering::MaterialDesc material;
					material.pipeline = extraPipeline;
					material.parameters =
					{
						{ "BaseColor", rendering::Vector4{ 0.4f, 1, 0.2f, 1 } },
						{ "BaseTexture", textureHandle },
						{ "ExtraTexture", textureHandle }
					};

					result = renderer.UpdateMaterial(materialHandle, material);
					if (!result.IsSuccess())
					{
						ShowRendererFailure(window, result);
						exitCode = 1;
						break;
					}
				}

				// Submit two animated cubes through the shared draw path.
				rendering::FrameRenderData frame;
				frame.time = seconds;
				frame.directionalLight = rendering::DirectionalLight{};

				rendering::RenderObject object{ geometryHandle, materialHandle };
				object.world = rendering::Matrix4x4::CreateRotationY(seconds) *
					rendering::Matrix4x4::CreateTranslation({ -0.45f, 0, 0 });
				frame.objects.push_back(object);

				object.world = rendering::Matrix4x4::CreateRotationX(seconds) *
					rendering::Matrix4x4::CreateTranslation({ 0.45f, 0, 0.25f });
				object.material = alternateMaterial;
				frame.objects.push_back(object);

				// Move the camera and apply the selected per-view overrides.
				rendering::RenderView view;
				view.output = windowOutput;
				view.cameraPosition = { std::sin(seconds * 0.2f) * 0.6f, 0.7f, -2.5f };
				view.view =
					rendering::Matrix4x4::CreateLookAt(view.cameraPosition, { 0, 0, 0 }, { 0, 1, 0 });
				view.projection = rendering::Matrix4x4::CreatePerspectiveFov(
					rendering::PI / 3, static_cast<float>(clientWidth) / clientHeight, 0.1f, 100);

				if (useMaterialOverride)
				{
					view.drawSettings.materialOverride = alternateMaterial;
				}

				if (usePipelineOverride)
				{
					view.drawSettings.pipelineOverride = extraPipeline;
				}

				frame.views.push_back(view);

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
