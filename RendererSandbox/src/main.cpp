#include "Diagnostics.h"
#include "ShaderFile.h"
#include "Renderer/Renderer.h"
#include "Renderer/Win32WindowOutputDesc.h"
#include "SkinningDemo.h"
#include "RendererAssets/AssetFiles.h"

#include <intrin.h>

#include <array>
#include <chrono>
#include <cmath>
#include <span>
#include <string>
#include <vector>
#include <utility>

namespace
{

	using namespace rendering;

	constexpr wchar_t WindowClassName[] = L"101RendererSandboxWindow";

	ResourceRegistrationResult<ShaderHandle> LoadAndRegisterShader(Renderer& renderer,
		const std::filesystem::path& shaderDirectory, const char* file, ShaderStage stage)
	{
		std::vector<std::byte> bytecode;
		ResourceRegistrationResult<ShaderHandle> loaded;
		loaded.result = assets::LoadShader(shaderDirectory, file, bytecode);

		if (!loaded.result.IsSuccess())
		{
			loaded.result.message = std::string(file) + ": " + loaded.result.message;
			return loaded;
		}

		ShaderDesc desc;
		desc.stage = stage;
		desc.bytecode = bytecode;
		return renderer.RegisterShader(desc);
	}

	Result UpdateTrianglePos(Renderer& renderer, BufferHandle buffer, float centerX, float centerY)
	{
		const std::array<std::array<float, 4>, 3> positions{ {
			{ centerX - 0.22f, centerY - 0.20f, 0.0f, 1.0f },
			{ centerX, centerY + 0.24f, 0.0f, 1.0f },
			{ centerX + 0.22f, centerY - 0.20f, 0.0f, 1.0f }
		} };

		return renderer.UpdateStructuredBuffer(buffer, std::as_bytes(std::span(positions)));
	}

	Result UpdateTriangleColor(Renderer& renderer, BufferHandle buffer, float r, float g, float b, float a)
	{
		const std::array<std::array<float, 4>, 1> color{ {
			{ r, g, b, a }
		} };

		return renderer.UpdateStructuredBuffer(buffer, std::as_bytes(std::span(color)));
	}

	struct Triangle
	{
		BufferHandle buffer;
		BufferHandle colorBuffer;
		MaterialHandle material;
	};

	Result PrepareTriangles(Renderer& renderer, std::array<Triangle, 2>& triangles)
	{
		std::filesystem::path shaderDirectory;
		try
		{
			shaderDirectory = sandbox::GetExecutableDirectory() / L"shaders";
		}
		catch (const std::exception&)
		{
			return {false, "Cannot resolve the Sandbox shader directory."};
		}

		const auto vertexShader = LoadAndRegisterShader(renderer, shaderDirectory, "TriangleVS.cso", ShaderStage::Vertex);
		if (!vertexShader.result.IsSuccess())
		{
			return vertexShader.result;
		}

		const auto pixelShader = LoadAndRegisterShader(renderer, shaderDirectory, "TrianglePS.cso", ShaderStage::Pixel);
		if (!pixelShader.result.IsSuccess())
		{
			return pixelShader.result;
		}

		std::array<ShaderMetadata, 2> metadata;
		Result reflected = renderer.GetShaderMetadata(vertexShader.handle, metadata[0]);
		if (!reflected.IsSuccess())
		{
			return reflected;
		}

		reflected = renderer.GetShaderMetadata(pixelShader.handle, metadata[1]);
		if (!reflected.IsSuccess())
		{
			return reflected;
		}

		PipelineDesc pipelineDesc;
		pipelineDesc.vertexShader = vertexShader.handle;
		pipelineDesc.pixelShader = pixelShader.handle;
		pipelineDesc.depth = DepthMode::Disabled;
		pipelineDesc.cull = CullMode::None;
		const auto pipeline = renderer.RegisterPipeline(pipelineDesc);
		if (!pipeline.result.IsSuccess())
		{
			return pipeline.result;
		}

		// A white texel keeps the animated color unchanged while exercising a texture SRV.
		const std::array pixels{std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
		TextureMipDesc mip;
		mip.pixels = pixels;
		mip.rowPitch = pixels.size();

		TextureDesc textureDesc;
		textureDesc.width = 1;
		textureDesc.height = 1;
		textureDesc.mips = std::span(&mip, 1);
		const auto texture = renderer.RegisterTexture(textureDesc);
		if (!texture.result.IsSuccess())
		{
			return texture.result;
		}

		// Reflection supplies each parameter name and stride. The demo supplies its data and element count.
		const ShaderResourceBinding* positions = nullptr;
		const ShaderResourceBinding* color = nullptr;
		MaterialDesc materialDesc;
		materialDesc.pipeline = pipeline.handle;
		for (const auto& stage : metadata)
		{
			for (const auto& resource : stage.resources)
			{
				if (resource.kind == ShaderResourceKind::Sampler)
				{
					continue;
				}

				if (resource.kind == ShaderResourceKind::StructuredBuffer && resource.name == "Positions")
				{
					positions = &resource;
				}
				else if (resource.kind == ShaderResourceKind::StructuredBuffer && resource.name == "Color")
				{
					color = &resource;
				}
				else if (resource.kind == ShaderResourceKind::Texture2D && resource.name == "DemoTexture")
				{
					MaterialParameter parameter;
					parameter.name = resource.name;
					parameter.value = texture.handle;
					materialDesc.parameters.push_back(std::move(parameter));
				}
				else
				{
					return {false, "Sandbox has no value for shader resource: " + resource.name};
				}
			}
		}

		if (!positions || !color || materialDesc.parameters.size() != 1)
		{
			return {false, "The Sandbox shaders must declare Positions, Color and DemoTexture."};
		}

		constexpr std::uint32_t float4Stride = sizeof(std::array<float, 4>);
		if (positions->elementStride != float4Stride || color->elementStride != float4Stride)
		{
			return {false, "The Sandbox position and color buffers require float4 elements."};
		}

		StructuredBufferDesc positionDesc;
		positionDesc.elementCount = 3;
		positionDesc.elementStride = positions->elementStride;
		StructuredBufferDesc colorDesc;
		colorDesc.elementCount = 1;
		colorDesc.elementStride = color->elementStride;
		constexpr std::array centers{-0.48f, 0.48f};

		for (std::size_t index = 0; index < triangles.size(); ++index)
		{
			const auto positionBuffer = renderer.CreateStructuredBuffer(positionDesc);
			if (!positionBuffer.result.IsSuccess())
			{
				return positionBuffer.result;
			}

			Result initialized = UpdateTrianglePos(renderer, positionBuffer.handle, centers[index], 0.0f);
			if (!initialized.IsSuccess())
			{
				return initialized;
			}

			const auto colorBuffer = renderer.CreateStructuredBuffer(colorDesc);
			if (!colorBuffer.result.IsSuccess())
			{
				return colorBuffer.result;
			}

			initialized = UpdateTriangleColor(renderer, colorBuffer.handle, 0.25f, 0.75f, 0.5f, 1.0f);
			if (!initialized.IsSuccess())
			{
				return initialized;
			}

			MaterialDesc triangleMaterial = materialDesc;
			MaterialParameter positionParameter;
			positionParameter.name = positions->name;
			positionParameter.value = positionBuffer.handle;
			triangleMaterial.parameters.push_back(std::move(positionParameter));

			MaterialParameter colorParameter;
			colorParameter.name = color->name;
			colorParameter.value = colorBuffer.handle;
			triangleMaterial.parameters.push_back(std::move(colorParameter));

			const auto material = renderer.RegisterMaterial(triangleMaterial);
			if (!material.result.IsSuccess())
			{
				return material.result;
			}

			triangles[index].buffer = positionBuffer.handle;
			triangles[index].colorBuffer = colorBuffer.handle;
			triangles[index].material = material.handle;
		}

		return {};
	}

	void ShowRendererFailure(HWND window, const Result& result)
	{
		sandbox::WriteDiagnostic(result.message);
		if (result.failureKind == FailureKind::GpuCompletionUnknown)
		{
			// The Renderer still owns resources that may be in flight.
			__fastfail(FAST_FAIL_FATAL_APP_EXIT);
		}

		const auto message = sandbox::Utf8ToWide(result.message);
		MessageBoxW(window, message.c_str(), L"RendererSandbox Error", MB_OK | MB_ICONERROR);
	}

	Result RunDemo(Renderer& renderer, HWND window, int showCommand, bool useSkinningDemo)
	{
		RendererConfiguration configuration;
		configuration.enableDebugLayer = true;
		configuration.enableGpuValidation = true;
		Result result = renderer.Initialize(configuration);
		if (!result.IsSuccess())
		{
			return result;
		}

		RECT clientRect{};
		if (!GetClientRect(window, &clientRect))
		{
			return { false, "GetClientRect failed." };
		}

		LONG width = clientRect.right - clientRect.left;
		LONG height = clientRect.bottom - clientRect.top;
		if (width <= 0 || height <= 0)
		{
			return { false, "The initial client area must have a positive size." };
		}

		Win32WindowOutputDesc outputDesc;
		outputDesc.window = window;
		outputDesc.width = static_cast<std::uint32_t>(width);
		outputDesc.height = static_cast<std::uint32_t>(height);
		const auto output = renderer.CreateWindowOutput(outputDesc);
		if (!output.status.IsSuccess())
		{
			return output.status;
		}

		std::array<Triangle, 2> triangles{};
		sandbox::SkinningDemo skinningDemo;
		if (useSkinningDemo)
		{
			std::filesystem::path shaderDirectory;
			try
			{
				shaderDirectory = sandbox::GetExecutableDirectory() / L"shaders";
			}
			catch (const std::exception&)
			{
				return { false, "Cannot resolve the Sandbox shader directory." };
			}
			const auto vs = LoadAndRegisterShader(renderer, shaderDirectory, "SkinnedMeshVS.cso", ShaderStage::Vertex);
			if (!vs.result.IsSuccess())
			{
				return vs.result;
			}

			const auto ps = LoadAndRegisterShader(renderer, shaderDirectory, "LitPS.cso", ShaderStage::Pixel);
			if (!ps.result.IsSuccess())
			{
				return ps.result;
			}

			result = sandbox::PrepareSkinningDemo(renderer, vs.handle, ps.handle, skinningDemo);
		}
		else
		{
			result = PrepareTriangles(renderer, triangles);
		}

		if (!result.IsSuccess())
		{
			return result;
		}

		ShowWindow(window, showCommand);
		const auto start = std::chrono::steady_clock::now();

		for (;;)
		{
			MSG message{};
			while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
			{
				if (message.message == WM_QUIT)
				{
					return {};
				}

				TranslateMessage(&message);
				DispatchMessageW(&message);
			}

			if (!GetClientRect(window, &clientRect))
			{
				return { false, "GetClientRect failed." };
			}

			const LONG nextWidth = clientRect.right - clientRect.left;
			const LONG nextHeight = clientRect.bottom - clientRect.top;

			if (IsIconic(window) || nextWidth <= 0 || nextHeight <= 0)
			{
				if (!WaitMessage())
				{
					return { false, "WaitMessage failed." };
				}
				continue;
			}

			if (nextWidth != width || nextHeight != height)
			{
				result = renderer.ResizeWindowOutput(output.handle, static_cast<std::uint32_t>(nextWidth),
					static_cast<std::uint32_t>(nextHeight));
				if (!result.IsSuccess())
				{
					return result;
				}

				width = nextWidth;
				height = nextHeight;
			}
			
			FrameRenderData frame;
			RenderView view;
			view.output = output.handle;

			if (useSkinningDemo)
			{
				if (GetAsyncKeyState('1') & 1)
				{
					result = sandbox::UpdateSkinningPose(renderer, skinningDemo.palettes[1], sandbox::SkinningPose::Bind);
				}
				else if (GetAsyncKeyState('2') & 1)
				{
					result = sandbox::UpdateSkinningPose(renderer, skinningDemo.palettes[1], sandbox::SkinningPose::Translation);
				}
				else if (GetAsyncKeyState('3') & 1)
				{
					result = sandbox::UpdateSkinningPose(renderer, skinningDemo.palettes[1], sandbox::SkinningPose::Blend);
				}

				sandbox::AppendSkinningDemo(frame, skinningDemo);
			}
			else
			{
				const float seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
				const float wave = std::sin(seconds);
				result = UpdateTrianglePos(renderer, triangles[1].buffer, 0.48f, wave * 0.4f);
				if (result.IsSuccess())
				{
					result = UpdateTriangleColor(renderer, triangles[1].colorBuffer,
						0.25f + wave * 0.25f, 0.75f - wave * 0.25f, 0.5f, 1.0f);
				}

				view.kind = ViewKind::Overlay;

				for (const auto& triangle : triangles)
				{
					ProceduralGeometry geometry;
					geometry.vertexCount = 3;

					RenderObject object;
					object.material = triangle.material;
					object.procedural = geometry;
					frame.objects.push_back(object);
				}
			}

			if (!result.IsSuccess())
			{
				return result;
			}

			frame.views.push_back(view);

			result = renderer.SubmitFrameRenderData(std::move(frame));
			if (!result.IsSuccess())
			{
				if (result.failureKind == FailureKind::GpuCompletionUnknown)
				{
					ShowRendererFailure(window, result);
				}
				return result;
			}

			const auto rendered = renderer.Render();
			if (!rendered.status.IsSuccess())
			{
				if (rendered.status.failureKind == FailureKind::GpuCompletionUnknown)
				{
					ShowRendererFailure(window, rendered.status);
				}
				return rendered.status;
			}

			if (!rendered.diagnostics.empty())
			{
				return { false, rendered.diagnostics.front().message };
			}

			// A Scene view also records the display transform draw.
			const std::size_t expectedDraws = useSkinningDemo ? skinningDemo.materials.size() + 1 : triangles.size();
			if (rendered.drawnObjects != expectedDraws)
			{
				return { false, "Unexpected Sandbox draw count." };
			}
		}
	}

	LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_CLOSE)
		{
			PostQuitMessage(0);
			return 0;
		}

		return DefWindowProcW(window, message, wParam, lParam);
	}

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand)
{
	const bool useSkinningDemo = std::wstring_view(commandLine) == L"--skinning";	WNDCLASSW windowClass{};
	
	windowClass.lpfnWndProc = WindowProcedure;
	windowClass.hInstance = instance;
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.lpszClassName = WindowClassName;

	if (!RegisterClassW(&windowClass))
	{
		MessageBoxW(nullptr, L"RegisterClassW failed.", L"RendererSandbox Error", MB_OK | MB_ICONERROR);
		return 1;
	}

	const HWND window = CreateWindowExW(0, WindowClassName,
		useSkinningDemo ?
		L"Skinning | 1: Bind  2: Translation  3: Blend" :
		L"StructuredBuffer Demo | Left: fixed | Right: animated",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1920, 1080,
		nullptr, nullptr, instance, nullptr);

	if (!window)
	{
		MessageBoxW(nullptr, L"CreateWindowExW failed.", L"RendererSandbox Error", MB_OK | MB_ICONERROR);
		UnregisterClassW(WindowClassName, instance);
		return 1;
	}

	int exitCode = 0;
	{
		Renderer renderer;
		const Result result = RunDemo(renderer, window, showCommand, true);
		if (!result.IsSuccess())
		{
			ShowRendererFailure(window, result);
			exitCode = 1;
		}
	}

	DestroyWindow(window);
	UnregisterClassW(WindowClassName, instance);
	return exitCode;
}
