// R7 implementation guide: section 4.39.
// Translate ImGui CPU DrawLists into existing Geometry/Material/common Draw input.
// CPU arrays are copied during registration/update, so later ImGui frames may reuse them.
// Native descriptors never become ImTextureID; image IDs map to logical resource handles.
// Window input, full Editor panels and separate OS viewport migration remain outside this probe.

#include "MinimalImGuiAdapter.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace rendering::probes
{

namespace
{

Result AdapterFailure(const std::string& message)
{
	Result failure;
	failure.succeeded = false;
	failure.message = "MinimalImGuiAdapter: " + message;
	return failure;
}

Result CompileProbeShader(const char* source, const char* profile, std::vector<std::byte>& bytecode)
{
	Microsoft::WRL::ComPtr<ID3DBlob> compiled;
	Microsoft::WRL::ComPtr<ID3DBlob> errors;
	const auto status = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
		"main", profile, 0, 0, &compiled, &errors);
	if (FAILED(status))
	{
		std::string message = "probe shader compilation failed";
		if (errors)
		{
			message.assign(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
		}
		return AdapterFailure(message);
	}

	const auto* begin = static_cast<const std::byte*>(compiled->GetBufferPointer());
	bytecode.assign(begin, begin + compiled->GetBufferSize());
	return {};
}

}

// Compile this probe's fixed client-side Shader once, then register ordinary Renderer resources.
// This is not Shader Reload in the Renderer. The static sampler convention uses s1 for LinearClamp.
// If any step fails, Shutdown still owns the responsibility to destroy preceding successful registrations.
Result MinimalImGuiAdapter::Initialize()
{
	const char* vertexSource = R"HLSL(
struct Input
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct Output
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

Output main(Input input)
{
    Output output;
    output.position = float4(input.position, 1);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}
)HLSL";
	const char* pixelSource = R"HLSL(
Texture2D Image : register(t0);
SamplerState LinearClamp : register(s1);

struct Input
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(Input input) : SV_TARGET
{
    return Image.Sample(LinearClamp, input.uv) * input.color;
}
)HLSL";

	std::vector<std::byte> bytecode;
	Result result = CompileProbeShader(vertexSource, "vs_5_0", bytecode);
	if (!result.IsSuccess())
	{
		return result;
	}

	ShaderDesc shaderDescription;
	shaderDescription.stage = ShaderStage::Vertex;
	shaderDescription.bytecode = bytecode;
	const auto vertex = m_renderer.RegisterShader(shaderDescription);
	if (!vertex.result.IsSuccess())
	{
		return vertex.result;
	}
	m_vertexShader = vertex.handle;

	result = CompileProbeShader(pixelSource, "ps_5_0", bytecode);
	if (!result.IsSuccess())
	{
		return result;
	}
	shaderDescription.stage = ShaderStage::Pixel;
	shaderDescription.bytecode = bytecode;
	const auto pixel = m_renderer.RegisterShader(shaderDescription);
	if (!pixel.result.IsSuccess())
	{
		return pixel.result;
	}
	m_pixelShader = pixel.handle;

	PipelineDesc pipelineDescription;
	pipelineDescription.vertexShader = m_vertexShader;
	pipelineDescription.pixelShader = m_pixelShader;
	pipelineDescription.blend = BlendMode::Alpha;
	pipelineDescription.depth = DepthMode::Disabled;
	pipelineDescription.cull = CullMode::None;
	const auto pipeline = m_renderer.RegisterPipeline(pipelineDescription);
	if (!pipeline.result.IsSuccess())
	{
		return pipeline.result;
	}
	m_pipeline = pipeline.handle;
	return {};
}

// Allocate ownership bookkeeping and Material input before registering the owned Material.
// If a later CPU map allocation threw after registration, the adapter could lose the handle to destroy.
// The identifier is published only after registration succeeds; external Scene outputs remain borrowed.
Result MinimalImGuiAdapter::AddImage(
	MaterialValue source,
	TextureHandle uploadedTexture,
	bool ownsTexture,
	ImTextureID& identifier)
{
	if (m_nextIdentifier == ImTextureID_Invalid || m_nextIdentifier == (std::numeric_limits<ImTextureID>::max)())
	{
		return AdapterFailure("image identifiers exhausted");
	}

	auto preparedImage = m_images.end();
	MaterialDesc description;
	try
	{
		// Allocate ownership bookkeeping before registering the owned Material.
		ImageEntry emptyEntry;
		const auto inserted = m_images.emplace(m_nextIdentifier, emptyEntry);
		if (!inserted.second)
		{
			return AdapterFailure("image identifier is already registered");
		}
		preparedImage = inserted.first;

		MaterialParameter parameter;
		parameter.name = "Image";
		parameter.value = std::move(source);
		description.pipeline = m_pipeline;
		description.parameters.push_back(std::move(parameter));
	}
	catch (const std::bad_alloc&)
	{
		if (preparedImage != m_images.end())
		{
			m_images.erase(preparedImage);
		}
		return AdapterFailure("image registration allocation failed");
	}
	catch (const std::length_error&)
	{
		if (preparedImage != m_images.end())
		{
			m_images.erase(preparedImage);
		}
		return AdapterFailure("image registration size is unsupported");
	}

	const auto material = m_renderer.RegisterMaterial(description);
	if (!material.result.IsSuccess())
	{
		m_images.erase(preparedImage);
		return material.result;
	}

	preparedImage->second.material = material.handle;
	preparedImage->second.uploadedTexture = uploadedTexture;
	preparedImage->second.ownsTexture = ownsTexture;
	identifier = m_nextIdentifier;
	++m_nextIdentifier;
	return {};
}

Result MinimalImGuiAdapter::RegisterImage(TextureHandle texture, ImTextureID& identifier)
{
	return AddImage(texture, texture, false, identifier);
}

Result MinimalImGuiAdapter::RegisterImage(RenderTargetTextureHandle texture, ImTextureID& identifier)
{
	TextureHandle unusedUploadedTexture;
	return AddImage(texture, unusedUploadedTexture, false, identifier);
}

Result MinimalImGuiAdapter::CopyTexturePixels(const ImTextureData& texture, std::vector<std::byte>& pixels) const
{
	if (!texture.Pixels || texture.Width <= 0 || texture.Height <= 0)
	{
		return AdapterFailure("texture pixels or extent unavailable");
	}
	if (texture.Format != ImTextureFormat_RGBA32 && texture.Format != ImTextureFormat_Alpha8)
	{
		return AdapterFailure("unsupported texture format");
	}

	const std::size_t width = static_cast<std::size_t>(texture.Width);
	const std::size_t height = static_cast<std::size_t>(texture.Height);
	if (width > 16384 || height > 16384)
	{
		return AdapterFailure("texture extent exceeds the supported 2D limit");
	}
	pixels.resize(width * height * 4);
	if (texture.Format == ImTextureFormat_RGBA32)
	{
		std::memcpy(pixels.data(), texture.Pixels, pixels.size());
		return {};
	}

	for (std::size_t pixelIndex = 0; pixelIndex < width * height; ++pixelIndex)
	{
		pixels[pixelIndex * 4] = std::byte{255};
		pixels[pixelIndex * 4 + 1] = std::byte{255};
		pixels[pixelIndex * 4 + 2] = std::byte{255};
		pixels[pixelIndex * 4 + 3] = static_cast<std::byte>(texture.Pixels[pixelIndex]);
	}
	return {};
}

// Translate ImGui Create/Update/Destroy requests into ordinary CPU Texture APIs.
// OK acknowledges CPU acceptance, not GPU completion. Update keeps the image ID and Texture handle.
// Destroy applies only to adapter-owned images, never borrowed Scene outputs.
Result MinimalImGuiAdapter::ProcessTexture(ImTextureData& texture)
{
	if (texture.Status == ImTextureStatus_OK || texture.Status == ImTextureStatus_Destroyed)
	{
		return {};
	}

	if (texture.Status == ImTextureStatus_WantDestroy)
	{
		const auto found = m_images.find(texture.TexID);
		if (found == m_images.end() || !found->second.ownsTexture)
		{
			return AdapterFailure("destroy request does not identify an owned texture");
		}

		Result result = m_renderer.DestroyMaterial(found->second.material);
		if (!result.IsSuccess())
		{
			return result;
		}
		result = m_renderer.DestroyTexture(found->second.uploadedTexture);
		if (!result.IsSuccess())
		{
			return result;
		}
		m_images.erase(found);
		texture.SetTexID(ImTextureID_Invalid);
		texture.SetStatus(ImTextureStatus_Destroyed);
		return {};
	}

	std::vector<std::byte> pixels;
	Result result = CopyTexturePixels(texture, pixels);
	if (!result.IsSuccess())
	{
		return result;
	}
	TextureMipDesc mip;
	mip.pixels = pixels;
	mip.rowPitch = static_cast<std::size_t>(texture.Width) * 4;
	TextureDesc description;
	description.width = static_cast<std::uint32_t>(texture.Width);
	description.height = static_cast<std::uint32_t>(texture.Height);
	description.mips = std::span<const TextureMipDesc>(&mip, 1);

	if (texture.Status == ImTextureStatus_WantCreate)
	{
		const auto registered = m_renderer.RegisterTexture(description);
		if (!registered.status.IsSuccess())
		{
			return registered.status;
		}
		ImTextureID identifier = ImTextureID_Invalid;
		result = AddImage(registered.handle, registered.handle, true, identifier);
		if (!result.IsSuccess())
		{
			const Result cleanup = m_renderer.DestroyTexture(registered.handle);
			(void)cleanup;
			return result;
		}
		texture.SetTexID(identifier);
	}
	else if (texture.Status == ImTextureStatus_WantUpdates)
	{
		const auto found = m_images.find(texture.TexID);
		if (found == m_images.end() || !found->second.ownsTexture)
		{
			return AdapterFailure("update request does not identify an owned texture");
		}
		// The CPU texture contains the complete image. The initial adapter uses
		// full replacement through the existing API rather than partial GPU copies.
		result = m_renderer.UpdateTexture(found->second.uploadedTexture, description);
		if (!result.IsSuccess())
		{
			return result;
		}
	}
	else
	{
		return AdapterFailure("unknown texture request");
	}

	texture.SetStatus(ImTextureStatus_OK);
	return {};
}

// Convert positions/UV/color and widen indices once per DrawList, sharing one Geometry across Commands.
// UpdateGeometry copies the arrays before ImGui can reuse them in its next frame.
// Prepare a handle slot before registration so allocation failure cannot leave an untracked Geometry.
Result MinimalImGuiAdapter::StoreDrawList(
	const ImDrawList& drawList,
	const ImDrawData& drawData,
	std::size_t listIndex)
{
	if (listIndex >= m_geometries.size())
	{
		try
		{
			// A successful registration must immediately have an owning Handle slot.
			m_geometries.resize(listIndex + 1);
		}
		catch (const std::bad_alloc&)
		{
			return AdapterFailure("geometry Handle storage allocation failed");
		}
		catch (const std::length_error&)
		{
			return AdapterFailure("geometry Handle storage size is unsupported");
		}
	}

	std::vector<MeshVertex> vertices;
	vertices.reserve(static_cast<std::size_t>(drawList.VtxBuffer.Size));
	for (const auto& source : drawList.VtxBuffer)
	{
		MeshVertex vertex;
		vertex.position.x = (source.pos.x - drawData.DisplayPos.x) * 2 / drawData.DisplaySize.x - 1;
		vertex.position.y = 1 - (source.pos.y - drawData.DisplayPos.y) * 2 / drawData.DisplaySize.y;
		vertex.uv.x = source.uv.x;
		vertex.uv.y = source.uv.y;
		vertex.color.x = static_cast<float>((source.col >> IM_COL32_R_SHIFT) & 255) / 255;
		vertex.color.y = static_cast<float>((source.col >> IM_COL32_G_SHIFT) & 255) / 255;
		vertex.color.z = static_cast<float>((source.col >> IM_COL32_B_SHIFT) & 255) / 255;
		vertex.color.w = static_cast<float>((source.col >> IM_COL32_A_SHIFT) & 255) / 255;
		vertices.push_back(vertex);
	}

	std::vector<std::uint32_t> indices;
	indices.reserve(static_cast<std::size_t>(drawList.IdxBuffer.Size));
	for (const auto index : drawList.IdxBuffer)
	{
		indices.push_back(index);
	}

	GeometryDesc description;
	description.vertices = vertices;
	description.indices = indices;
	if (m_geometries[listIndex] != GeometryHandle{})
	{
		return m_renderer.UpdateGeometry(m_geometries[listIndex], description);
	}

	const auto registered = m_renderer.RegisterGeometry(description);
	if (!registered.status.IsSuccess())
	{
		return registered.status;
	}
	m_geometries[listIndex] = registered.handle;
	return {};
}

// Build new RenderObjects locally and append only when conversion succeeds.
// Resource updates accepted earlier in this call are not transactionally rolled back with the Object list.
// DisplayPos and FramebufferScale belong to this client adapter, not the Renderer core.
// Reject separate OS Viewports and arbitrary callbacks instead of hiding undeclared GPU work.
Result MinimalImGuiAdapter::Append(const ImDrawData& drawData, ViewId view, FrameRenderData& frame)
{
	if (!drawData.Valid)
	{
		return AdapterFailure("draw data is not valid");
	}
	if (drawData.OwnerViewport)
	{
		if (!ImGui::GetCurrentContext())
		{
			return AdapterFailure("owned Viewport requires an active ImGui context");
		}
		if (drawData.OwnerViewport != ImGui::GetMainViewport())
		{
			return AdapterFailure("separate OS Window multi-viewports are unsupported");
		}
	}
	const bool invalidSize = drawData.DisplaySize.x <= 0 || drawData.DisplaySize.y <= 0;
	const bool invalidScale = drawData.FramebufferScale.x <= 0 || drawData.FramebufferScale.y <= 0;
	if (invalidSize || invalidScale)
	{
		return AdapterFailure("display extent and framebuffer scale must be positive");
	}
	const bool finiteOrigin = std::isfinite(drawData.DisplayPos.x) && std::isfinite(drawData.DisplayPos.y);
	const bool finiteSize = std::isfinite(drawData.DisplaySize.x) && std::isfinite(drawData.DisplaySize.y);
	const bool finiteScale = std::isfinite(drawData.FramebufferScale.x) && std::isfinite(drawData.FramebufferScale.y);
	if (!finiteOrigin || !finiteSize || !finiteScale)
	{
		return AdapterFailure("display transform must be finite");
	}
	const float framebufferWidth = drawData.DisplaySize.x * drawData.FramebufferScale.x;
	const float framebufferHeight = drawData.DisplaySize.y * drawData.FramebufferScale.y;
	if (framebufferWidth > 16384 || framebufferHeight > 16384)
	{
		return AdapterFailure("framebuffer extent exceeds the supported 2D limit");
	}

	if (drawData.Textures)
	{
		for (auto* texture : *drawData.Textures)
		{
			Result result = ProcessTexture(*texture);
			if (!result.IsSuccess())
			{
				return result;
			}
		}
	}

	std::vector<RenderObject> objects;
	for (int listIndex = 0; listIndex < drawData.CmdListsCount; ++listIndex)
	{
		const auto& drawList = *drawData.CmdLists[listIndex];
		for (const auto& command : drawList.CmdBuffer)
		{
			const bool unsupportedCallback = command.UserCallback &&
				command.UserCallback != ImDrawCallback_ResetRenderState;
			if (unsupportedCallback)
			{
				return AdapterFailure("arbitrary draw callbacks are unsupported");
			}
		}
		if (drawList.VtxBuffer.empty() || drawList.IdxBuffer.empty())
		{
			continue;
		}
		Result result = StoreDrawList(drawList, drawData, static_cast<std::size_t>(listIndex));
		if (!result.IsSuccess())
		{
			return result;
		}

		for (const auto& command : drawList.CmdBuffer)
		{
			if (command.UserCallback)
			{
				if (command.UserCallback == ImDrawCallback_ResetRenderState)
				{
					// Each shared Draw records all required state itself.
					continue;
				}
				return AdapterFailure("arbitrary draw callbacks are unsupported");
			}
			if (command.ElemCount == 0)
			{
				continue;
			}

			const auto image = m_images.find(command.GetTexID());
			if (image == m_images.end())
			{
				return AdapterFailure("draw references an unknown image identifier");
			}
			if (command.VtxOffset > static_cast<unsigned>((std::numeric_limits<std::int32_t>::max)()))
			{
				return AdapterFailure("base vertex is unsupported");
			}
			const bool finiteMinimum = std::isfinite(command.ClipRect.x) && std::isfinite(command.ClipRect.y);
			const bool finiteMaximum = std::isfinite(command.ClipRect.z) && std::isfinite(command.ClipRect.w);
			if (!finiteMinimum || !finiteMaximum)
			{
				return AdapterFailure("clip rectangle must be finite");
			}
			const float clipLeft = (command.ClipRect.x - drawData.DisplayPos.x) * drawData.FramebufferScale.x;
			const float clipTop = (command.ClipRect.y - drawData.DisplayPos.y) * drawData.FramebufferScale.y;
			const float clipRight = (command.ClipRect.z - drawData.DisplayPos.x) * drawData.FramebufferScale.x;
			const float clipBottom = (command.ClipRect.w - drawData.DisplayPos.y) * drawData.FramebufferScale.y;

			ScissorRect scissor;
			scissor.left = static_cast<std::int32_t>(std::floor(std::clamp(clipLeft, 0.0f, framebufferWidth)));
			scissor.top = static_cast<std::int32_t>(std::floor(std::clamp(clipTop, 0.0f, framebufferHeight)));
			scissor.right = static_cast<std::int32_t>(std::ceil(std::clamp(clipRight, 0.0f, framebufferWidth)));
			scissor.bottom = static_cast<std::int32_t>(std::ceil(std::clamp(clipBottom, 0.0f, framebufferHeight)));
			if (scissor.right <= scissor.left || scissor.bottom <= scissor.top)
			{
				continue;
			}

			RenderObject object;
			object.geometry = m_geometries[static_cast<std::size_t>(listIndex)];
			object.material = image->second.material;
			object.targetViews.push_back(view);
			object.drawRange.first = command.IdxOffset;
			object.drawRange.count = command.ElemCount;
			object.drawRange.baseVertex = static_cast<std::int32_t>(command.VtxOffset);
			object.scissor = scissor;
			objects.push_back(object);
		}
	}

	frame.objects.insert(frame.objects.end(), objects.begin(), objects.end());
	return {};
}

// Request destruction of all registrations owned by this adapter and keep the first failure.
// Continue cleanup after an individual failure; borrowed Scene outputs are not destroyed.
// Renderer Fence/retirement logic decides when physical resources may actually be released.
Result MinimalImGuiAdapter::Shutdown()
{
	Result firstFailure;
	const auto rememberFailure = [&firstFailure](Result result)
	{
		if (firstFailure.IsSuccess() && !result.IsSuccess())
		{
			firstFailure = std::move(result);
		}
	};

	for (const auto& entry : m_images)
	{
		rememberFailure(m_renderer.DestroyMaterial(entry.second.material));
		if (entry.second.ownsTexture)
		{
			rememberFailure(m_renderer.DestroyTexture(entry.second.uploadedTexture));
		}
	}
	for (const auto geometry : m_geometries)
	{
		if (geometry != GeometryHandle{})
		{
			rememberFailure(m_renderer.DestroyGeometry(geometry));
		}
	}
	if (m_pipeline != PipelineHandle{})
	{
		rememberFailure(m_renderer.DestroyPipeline(m_pipeline));
	}
	if (m_vertexShader != ShaderHandle{})
	{
		rememberFailure(m_renderer.DestroyShader(m_vertexShader));
	}
	if (m_pixelShader != ShaderHandle{})
	{
		rememberFailure(m_renderer.DestroyShader(m_pixelShader));
	}

	m_images.clear();
	m_geometries.clear();
	m_pipeline = PipelineHandle{};
	m_vertexShader = ShaderHandle{};
	m_pixelShader = ShaderHandle{};
	return firstFailure;
}

}
