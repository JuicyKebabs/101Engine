// R7 implementation guide: section 4.22.
// Resolve original Objects against the final View Pipeline, then prepare common DrawCommands.
// Registered Geometry and explicit procedural drawing share this path without invalid-handle fallback.
// View-derived constants, sampling, range and scissor are prepared before native draw recording.

#include "DrawSystem.h"
#include "FrameDrawContext.h"
#include "Backend/DrawPacket.h"
#include "Backend/PipelineGpu.h"
#include "Graph/RenderPassContext.h"
#include "Backend/FrameResources.h"
#include "Math/Finite.h"
#include "Failure.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <d3dcompiler.h>
#include <d3d12shader.h>
#include <cctype>
#include <memory>
#include <type_traits>

namespace rendering::detail
{

namespace
{

	Result ValidateLight(const DirectionalLight& light)
	{
		if (!Finite(light.lightDirection) ||
			!Finite(light.lightColor) ||
			!Finite(light.ambient))
		{
			return { false, "ValidateLight: non-finite parameters." };
		}

		const auto& direction = light.lightDirection;

		const float lengthSquared =
			direction[0] * direction[0] +
			direction[1] * direction[1] +
			direction[2] * direction[2];

		if (lengthSquared < 1e-12f)
		{
			return { false, "ValidateLight: direction is too small." };
		}

		return {};
	}

	Result ValidatePipelineDesc(const PipelineDesc& desc)
	{
		if (desc.blend != BlendMode::Opaque && desc.blend != BlendMode::Alpha)
		{
			return { false, "ValidatePipelineDesc: unsupported blend mode." };
		}

		if (desc.depth != DepthMode::Disabled && desc.depth != DepthMode::ReadOnly && desc.depth != DepthMode::ReadWrite)
		{
			return { false, "ValidatePipelineDesc: unsupported depth mode." };
		}

		if (desc.cull != CullMode::None && desc.cull != CullMode::Back && desc.cull != CullMode::Front)
		{
			return { false, "ValidatePipelineDesc: unsupported cull mode." };
		}

		if (desc.blend == BlendMode::Alpha && desc.depth == DepthMode::ReadWrite)
		{
			return { false, "ValidatePipelineDesc: Transparent Pipeline must disable Depth writes." };
		}

		return {};
	}

	struct VertexField
	{
		const char* semantic;
		UINT count;
		UINT offset;
		DXGI_FORMAT format;
	};

	std::vector<VertexField> VertexFields()
	{
		MeshVertex vertex;

		auto offset = [&](const void* member)
			{
				return static_cast<UINT>(static_cast<const char*>(member) - reinterpret_cast<const char*>(&vertex));
			};

		std::vector<VertexField> fields;
		fields.push_back(VertexField{ "POSITION", 3, offset(&vertex.position), DXGI_FORMAT_R32G32B32_FLOAT });
		fields.push_back(VertexField{ "NORMAL", 3, offset(&vertex.normal), DXGI_FORMAT_R32G32B32_FLOAT });
		fields.push_back(VertexField{ "TEXCOORD", 2, offset(&vertex.uv), DXGI_FORMAT_R32G32_FLOAT });
		fields.push_back(VertexField{ "TANGENT", 3, offset(&vertex.tangent), DXGI_FORMAT_R32G32B32_FLOAT });
		fields.push_back(VertexField{ "COLOR", 4, offset(&vertex.color), DXGI_FORMAT_R32G32B32A32_FLOAT });

		return fields;
	}

	// Validate that the vertex shader outputs match the pixel shader inputs for all non-system value semantics.
	Result ValidateShaderLink(const ShaderContent& vertex, const ShaderContent* pixel)
	{
		if (!pixel)
		{
			return {};
		}

		// Use D3DReflect to obtain shader reflection interfaces for both vertex and pixel shaders.
		Microsoft::WRL::ComPtr<ID3D12ShaderReflection> vertexReflection;
		auto vsHr = D3DReflect(vertex.bytecode.data(), vertex.bytecode.size(), IID_PPV_ARGS(&vertexReflection));

		if (FAILED(vsHr))
		{
			return { false, "ValidateShaderLink: Vertex shader reflection failed." };
		}

		Microsoft::WRL::ComPtr<ID3D12ShaderReflection> pixelReflection;
		auto psHr = D3DReflect(pixel->bytecode.data(), pixel->bytecode.size(), IID_PPV_ARGS(&pixelReflection));

		if (FAILED(psHr))
		{
			return { false, "ValidateShaderLink: Pixel shader reflection failed." };
		}

		// Retrieve the shader descriptions for both vertex and pixel shaders.
		D3D12_SHADER_DESC vertexDesc{};

		if (FAILED(vertexReflection->GetDesc(&vertexDesc)))
		{
			return { false, "ValidateShaderLink: Vertex shader reflection description failed." };
		}

		D3D12_SHADER_DESC pixelDesc{};

		if (FAILED(pixelReflection->GetDesc(&pixelDesc)))
		{
			return { false, "ValidateShaderLink: Pixel shader reflection description failed." };
		}

		// Helper function to convert semantic names to uppercase for case-insensitive comparison.
		auto canonical = [](const char* name)
			{
				std::string nameStr = name;

				for (char& character : nameStr)
				{
					character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
				}

				return nameStr;
			};

		// Iterate through each input parameter of the pixel shader.
		for (UINT i = 0; i < pixelDesc.InputParameters; ++i)
		{
			// Get and validate the input parameter description of the pixel shader for the current index.
			D3D12_SIGNATURE_PARAMETER_DESC input{};

			if (FAILED(pixelReflection->GetInputParameterDesc(i, &input)))
			{
				return { false, "ValidateShaderLink: Pixel shader linkage lookup failed" };
			}

			if (!input.ReadWriteMask || (input.SystemValueType != D3D_NAME_UNDEFINED && input.SystemValueType != D3D_NAME_POSITION))
			{
				continue;
			}

			bool match = false;

			// Iterate through each output parameter of the vertex shader.
			for (UINT j = 0; j < vertexDesc.OutputParameters; ++j)
			{
				// Get and validate the output parameter description of the vertex shader for the current index.
				D3D12_SIGNATURE_PARAMETER_DESC output{};

				if (FAILED(vertexReflection->GetOutputParameterDesc(j, &output)))
				{
					return { false, "ValidateShaderLink: Vertex shader linkage lookup failed" };
				}

				// Check if the semantic names, indices, component types, and read/write masks 
				// match between the vertex shader output and pixel shader input.
				if (canonical(input.SemanticName) == canonical(output.SemanticName) &&
					input.SemanticIndex == output.SemanticIndex &&
					input.ComponentType == output.ComponentType &&
					(input.ReadWriteMask & ~output.Mask) == 0)
				{
					match = true;
				}
			}

			if (!match)
			{
				return { false, "ValidateShaderLink: VS/PS semantic mismatch: " + std::string(input.SemanticName) };
			}
		}

		return {};
	}


	// Explicit procedural input must not conceal a missing/destroyed Mesh.
	// For indexed Geometry validate actual selected index + signed baseVertex using widened arithmetic.
	// An in-range Index Buffer slice alone cannot guarantee that Vertex Buffer references are in range.
	Result ValidateGeometrySelection(const RenderObject& object, const DrawInputs& inputs)
	{
		if (object.procedural)
		{
			if (object.geometry != GeometryHandle{} || object.procedural->vertexCount == 0)
			{
				return Fail("Procedural drawing requires an explicit count and no GeometryHandle.");
			}

			const bool usesRangeOffset = object.drawRange.first != 0 || object.drawRange.baseVertex != 0;
			if (usesRangeOffset || object.drawRange.count != 0)
			{
				return Fail("Procedural drawing takes its count from ProceduralGeometry only.");
			}

			if (!inputs.vertexShader->metadata.vertexInputs.empty())
			{
				return Fail("Procedural drawing cannot satisfy Vertex Buffer inputs.");
			}

			return {};
		}

		const auto& geometry = inputs.geometry->cpuData;
		const bool indexed = !geometry.indices.empty();
		const std::size_t availableCount = indexed ? geometry.indices.size() : geometry.vertices.size();
		if (object.drawRange.first > availableCount)
		{
			return Fail("Draw range starts beyond the Geometry.");
		}

		const std::size_t remainingCount = availableCount - object.drawRange.first;
		const std::size_t selectedCount = object.drawRange.count == 0 ? remainingCount : object.drawRange.count;
		if (selectedCount == 0 || selectedCount > remainingCount)
		{
			return Fail("Draw range is empty or exceeds the Geometry.");
		}

		if (!indexed && object.drawRange.baseVertex != 0)
		{
			return Fail("Base vertex is only supported for indexed Geometry.");
		}

		if (indexed)
		{
			for (std::size_t index = 0; index < selectedCount; ++index)
			{
				const std::uint32_t rawIndex = geometry.indices[object.drawRange.first + index];
				const std::int64_t vertexIndex = static_cast<std::int64_t>(rawIndex) + object.drawRange.baseVertex;
				if (vertexIndex < 0 || static_cast<std::uint64_t>(vertexIndex) >= geometry.vertices.size())
				{
					return Fail("Index plus base vertex is outside the Geometry.");
				}
			}
		}

		return {};
	}

	// Derive a View-local draw World without mutating the submitted Object.
	// Spherical uses Camera up from inverse View; Cylindrical keeps world Y upright.
	// Coincident/pole cases use deterministic fallbacks; arbitrary shear/reflection preservation is not generalized.
	Result DeriveObjectWorld(const RenderObject& object, const RenderView& view, Matrix4x4& output)
	{
		output = object.world;
		if (object.billboard == BillboardMode::None)
		{
			return {};
		}

		const float viewDeterminant = view.view.Determinant();
		if (!std::isfinite(viewDeterminant) || std::abs(viewDeterminant) < 1e-12f)
		{
			return Fail("Billboard requires an invertible View matrix.");
		}

		// Camera up must come from this View, including Camera roll.
		// Using a global main Camera would mix facing between Views; using world Y here would lose spherical roll.
		const Matrix4x4 cameraWorld = view.view.Inverse();
		Vector3 cameraUp;
		cameraUp.x = cameraWorld._21;
		cameraUp.y = cameraWorld._22;
		cameraUp.z = cameraWorld._23;
		cameraUp = cameraUp.Normalized();

		// The spherical helper needs a nonparallel Up/forward pair.
		// Choose a deterministic perpendicular axis at the camera/object pole.
		const Vector3 objectPosition = object.world.GetTranslation();
		const Vector3 cameraDirection = view.cameraPosition - objectPosition;
		if (object.billboard == BillboardMode::Spherical && cameraDirection.LengthSq() > 1e-6f)
		{
			const Vector3 forward = cameraDirection.Normalized();
			if (cameraUp.Cross(forward).LengthSq() <= 1e-6f)
			{
				cameraUp = std::abs(forward.y) < 0.9f ? Vector3::Up() : Vector3::Right();
			}
		}

		Matrix4x4 sourceWorld = object.world;
		if (object.billboard == BillboardMode::Spherical)
		{
			output = sourceWorld.ToBillboard(view.cameraPosition, cameraUp);
		}
		else if (object.billboard == BillboardMode::Cylindrical)
		{
			output = sourceWorld.ToCylindricalBillboard(view.cameraPosition, cameraUp);
		}
		else
		{
			return Fail("Unsupported Billboard mode.");
		}

		const float resultDeterminant = output.Determinant();
		if (!Finite(output) || !std::isfinite(resultDeterminant) || std::abs(resultDeterminant) < 1e-12f)
		{
			return Fail("Billboard produced an invalid World matrix.");
		}

		return {};
	}

	// Intersect output bounds, Viewport and optional Object clip rectangle in pixel coordinates.
	// Round outward for fractional viewport edges. PrepareCommands removes an empty result before recording.
	// The native path still sets a rectangle for an Object with no explicit scissor.
	D3D12_RECT DrawScissor(const RenderObject& object, const Viewport& viewport, std::uint32_t width, std::uint32_t height)
	{
		D3D12_RECT scissor{};
		scissor.left = (std::max)(LONG{ 0 }, static_cast<LONG>(std::floor(viewport.x)));
		scissor.top = (std::max)(LONG{ 0 }, static_cast<LONG>(std::floor(viewport.y)));
		scissor.right = (std::min)(static_cast<LONG>(width), static_cast<LONG>(std::ceil(viewport.x + viewport.width)));
		scissor.bottom = (std::min)(static_cast<LONG>(height), static_cast<LONG>(std::ceil(viewport.y + viewport.height)));
		if (object.scissor)
		{
			scissor.left = (std::max)(scissor.left, static_cast<LONG>(object.scissor->left));
			scissor.top = (std::max)(scissor.top, static_cast<LONG>(object.scissor->top));
			scissor.right = (std::min)(scissor.right, static_cast<LONG>(object.scissor->right));
			scissor.bottom = (std::min)(scissor.bottom, static_cast<LONG>(object.scissor->bottom));
		}

		return scissor;
	}

	DXGI_FORMAT NativeFormat(graph::GraphTextureFormat format)
	{
		switch (format)
		{
		case graph::GraphTextureFormat::RGBA8_UNorm:
			return DXGI_FORMAT_R8G8B8A8_UNORM;

		case graph::GraphTextureFormat::RGBA8_sRGB:
			return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

		case graph::GraphTextureFormat::D32_Float:
			return DXGI_FORMAT_D32_FLOAT;

		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}

}

Result ValidateFrameInputs(const FrameRenderData& data)
{
	if (!std::isfinite(data.time))
	{
		return { false, "ValidateFrameInputs: invalid Frame time input" };
	}

	if (data.directionalLight.has_value())
	{
		auto checked = ValidateLight(data.directionalLight.value());

		if (!checked.IsSuccess())
		{
			return { false, "ValidateFrameInputs: invalid Frame light input" };
		}
	}

	for (const auto& view : data.views)
	{
		if (!Finite(view.view) || !Finite(view.projection) ||
			!Finite(Vector4{ view.cameraPosition.x, view.cameraPosition.y, view.cameraPosition.z, 0 }))
		{
			return { false, "ValidateFrameInputs: invalid Frame view input" };
		}
	}

	return {};
}

ResourceRegistrationResult<ShaderHandle> DrawResources::RegisterShader(const ShaderDesc& desc)
{
	auto candidate = std::make_unique<ShaderContent>();

	auto result = ReflectShader(desc, *candidate);

	if (!result.IsSuccess())
	{
		return { result, {} };
	}

	return shaders.Register(std::move(candidate));
}

ResourceRegistrationResult<PipelineHandle> DrawResources::RegisterPipeline(const PipelineDesc& desc)
{
	auto checked = ValidatePipelineDesc(desc);

	if (!checked.IsSuccess())
	{
		return { checked, {} };
	}

	auto* vs = shaders.FindForNewUse(desc.vertexShader);
	auto* ps = shaders.FindForNewUse(desc.pixelShader);

	if (!vs || vs->metadata.stage != ShaderStage::Vertex)
	{
		return { { false, "RegisterPipeline: Vertex shader handle is invalid or not a vertex shader." }, {} };
	}

	if (desc.pixelShader != ShaderHandle{} && (!ps || ps->metadata.stage != ShaderStage::Pixel))
	{
		return { { false, "RegisterPipeline: Pixel shader handle is invalid or not a pixel shader." }, {} };
	}

	checked = ValidateShaderLink(*vs, ps);

	if (!checked.IsSuccess())
	{
		return { checked, {} };
	}

	auto candidate = std::make_unique<PipelineContent>();

	candidate->desc = desc;

	checked = MergeMetadata(vs->metadata, ps ? &ps->metadata : nullptr, candidate->layout);

	if (!checked.IsSuccess())
	{
		return { checked, {} };
	}

	auto fields = VertexFields();

	for (const auto& input : vs->metadata.vertexInputs)
	{
		bool mismatchVertex = std::none_of(fields.begin(), fields.end(), [&](const VertexField& field)
			{
				return input.semantic == field.semantic && input.components == field.count;
			});

		if (input.index != 0 || mismatchVertex)
		{
			return { { false, "RegisterPipeline: MeshVertex input mismatch: " + input.semantic }, {} };
		}
	}

	const auto tables = std::count_if(
		candidate->layout.resources.begin(), candidate->layout.resources.end(),
		[](const auto& binding)
		{
			return !binding.sampler;
		});

	if (candidate->layout.constants.size() * 2 + tables > 64)
	{
		return { {false, "RegisterPipeline: Root Signature exceeds 64 DWORDs"}, {} };
	}

	return pipelines.Register(std::move(candidate));
}

ResourceRegistrationResult<MaterialHandle> DrawResources::RegisterMaterial(const MaterialDesc& desc)
{
	auto checked = ValidateMaterial(desc);

	if (!checked.IsSuccess())
	{
		return { checked, {} };
	}

	if (!pipelines.FindForNewUse(desc.pipeline))
	{
		return { { false, "RegisterMaterial: Pipeline handle is invalid." }, {} };
	}

	auto candidate = std::make_unique<MaterialContent>();
	candidate->desc = desc;

	return materials.Register(std::move(candidate));
}

Result DrawResources::UpdateMaterial(MaterialHandle handle, const MaterialDesc& desc)
{
	auto checked = ValidateMaterial(desc);

	if (!checked.IsSuccess())
	{
		return checked;
	}

	if (!pipelines.FindForNewUse(desc.pipeline))
	{
		return { false, "UpdateMaterial: Pipeline handle is invalid." };
	}

	auto candidate = std::make_unique<MaterialContent>();
	candidate->desc = desc;

	return materials.Update(handle, std::move(candidate));
}

Result DrawResources::ResolveTextureInput(
	const MaterialDesc& material,
	const ShaderResourceBinding& requirement,
	TextureRegistry& textures,
	TextureHandle& handle,
	TextureContent*& content) const
{
	const auto parameter = std::find_if(material.parameters.begin(), material.parameters.end(),
		[&requirement](const MaterialParameter& value)
		{
			return value.name == requirement.name;
		});

	if (parameter == material.parameters.end())
	{
		return { false, "Missing Texture2D parameter: " + requirement.name };
	}

	if (!std::holds_alternative<TextureHandle>(parameter->value))
	{
		return { false, "Expected Texture2D parameter: " + requirement.name };
	}

	const TextureHandle selectedHandle = std::get<TextureHandle>(parameter->value);
	if (!textures.FindForNewUse(selectedHandle))
	{
		return { false, "Texture unavailable: " + requirement.name };
	}

	handle = selectedHandle;
	content = textures.FindRegisteredContent(selectedHandle);
	return {};
}

// Use the Material value's handle type to select the correct Registry owner.
// This operation creates no read-only alias handle, descriptor ownership or duplicated GPU image.
Result DrawResources::ResolveSampledTexture(
	const MaterialDesc& material,
	const ShaderResourceBinding& requirement,
	TextureRegistry& textures,
	RenderTargetTextureRegistry& targets,
	SampledTexture& sampled) const
{
	const auto parameter = std::find_if(material.parameters.begin(), material.parameters.end(),
		[&requirement](const MaterialParameter& value)
		{
			return value.name == requirement.name;
		});

	if (parameter == material.parameters.end())
	{
		return Fail("Missing Texture2D parameter: " + requirement.name);
	}

	SampledTexture selected;
	if (const auto* targetHandle = std::get_if<RenderTargetTextureHandle>(&parameter->value))
	{
		selected.targetHandle = *targetHandle;
		selected.target = targets.FindForNewUse(*targetHandle);
		if (!selected.target || !selected.target->gpuData)
		{
			return Fail("RenderTargetTexture unavailable: " + requirement.name);
		}
	}
	else
	{
		Result resolved = ResolveTextureInput(material, requirement, textures, selected.textureHandle, selected.texture);
		if (!resolved.IsSuccess())
		{
			return resolved;
		}
	}

	sampled = selected;
	return {};
}

// Resolve the final Pipeline after applying View overrides, then validate its complete input contract.
// Vertex deformation and Texture Alpha inputs must remain available in a compatible override.
// A failed Object resolution produces a diagnostic/skip; it is not converted to a fallback draw.
Result DrawResources::ResolveDrawInputs(
	const RenderObject& object,
	std::size_t objectIndex,
	const DrawSettings& settings,
	GeometryRegistry& geometries,
	TextureRegistry& textures,
	DrawInputs& inputs,
	DrawDiagnostic& diagnostic,
	RenderTargetTextureRegistry* targets)
{
	diagnostic.objectIndex = objectIndex;
	diagnostic.geometry = object.geometry;
	diagnostic.material = settings.materialOverride.value_or(object.material);

	DrawInputs selected;
	if (!object.procedural)
	{
		if (!geometries.FindForNewUse(object.geometry))
		{
			return Fail("Geometry unavailable.");
		}

		selected.geometry = geometries.FindRegisteredContent(object.geometry);
		diagnostic.geometryRevision = selected.geometry->revision;
	}

	selected.material = materials.FindForNewUse(diagnostic.material);
	if (!selected.material)
	{
		return { false, "Material unavailable." };
	}
	diagnostic.materialRevision = selected.material->revision;

	diagnostic.pipeline = settings.pipelineOverride.value_or(selected.material->desc.pipeline);
	selected.pipeline = pipelines.FindForNewUse(diagnostic.pipeline);
	if (!selected.pipeline)
	{
		return { false, "Pipeline unavailable." };
	}

	diagnostic.vertexShader = selected.pipeline->desc.vertexShader;
	diagnostic.pixelShader = selected.pipeline->desc.pixelShader;
	selected.vertexShader = shaders.FindForNewUse(diagnostic.vertexShader);
	selected.pixelShader = shaders.FindForNewUse(diagnostic.pixelShader);
	const bool requiredPixelMissing = diagnostic.pixelShader != ShaderHandle{} && !selected.pixelShader;
	if (!selected.vertexShader || requiredPixelMissing)
	{
		return { false, "Dependent Shader unavailable." };
	}
	diagnostic.vertexRevision = selected.vertexShader->revision;
	diagnostic.pixelRevision = selected.pixelShader ? selected.pixelShader->revision : 0;

	if (!Finite(object.world) || !Finite(object.tint))
	{
		return { false, "World and Tint must be finite." };
	}

	const float worldDeterminant = object.world.Determinant();
	if (!std::isfinite(worldDeterminant) || std::abs(worldDeterminant) < 1e-12f)
	{
		return { false, "World must be invertible." };
	}

	const bool supportedBillboard = object.billboard == BillboardMode::None ||
		object.billboard == BillboardMode::Spherical || object.billboard == BillboardMode::Cylindrical;
	if (!supportedBillboard)
	{
		return Fail("Unsupported Billboard mode.");
	}

	if (object.scissor)
	{
		if (object.scissor->right < object.scissor->left || object.scissor->bottom < object.scissor->top)
		{
			return Fail("Scissor coordinates are reversed.");
		}
	}

	Result geometryChecked = ValidateGeometrySelection(object, selected);
	if (!geometryChecked.IsSuccess())
	{
		return geometryChecked;
	}

	// Validate logical inputs without retaining a packed copy for every Object.
	for (const auto& buffer : selected.pipeline->layout.constants)
	{
		if (buffer.binding != 5)
		{
			continue;
		}

		for (const auto& variable : buffer.variables)
		{
			if (!variable.used)
			{
				continue;
			}

			const auto parameter = std::find_if(
				selected.material->desc.parameters.begin(), selected.material->desc.parameters.end(),
				[&variable](const MaterialParameter& value)
				{
					return value.name == variable.name;
				});

			if (parameter == selected.material->desc.parameters.end())
			{
				return { false, "Missing Material parameter: " + variable.name };
			}

			if (TypeOf(parameter->value) != variable.type)
			{
				return { false, "Material type mismatch: " + variable.name };
			}
		}
	}

	for (const auto& requirement : selected.pipeline->layout.resources)
	{
		if (requirement.sampler)
		{
			continue;
		}

		Result resolved;
		if (targets)
		{
			SampledTexture sampled;
			resolved = ResolveSampledTexture(selected.material->desc, requirement, textures, *targets, sampled);
		}
		else
		{
			TextureHandle handle;
			TextureContent* content = nullptr;
			resolved = ResolveTextureInput(selected.material->desc, requirement, textures, handle, content);
		}

		if (!resolved.IsSuccess())
		{
			return resolved;
		}
	}

	inputs = selected;
	return {};
}

// Allocate this View's Camera and per-Object data in distinct frame storage.
// Common allocations may be shared inside the current preparation call, never across different Views.
// Compute both World and inverse-transpose normal data from the derived Billboard World.
Result DrawResources::PrepareConstantBindings(
	const FrameRenderData& frame,
	const RenderView& view,
	const RenderObject& object,
	const DrawInputs& inputs,
	backend::Backend& backend,
	std::array<backend::ConstantAllocation, 4>& commonAllocations,
	DrawCommand& command)
{
	struct ViewConstants
	{
		Matrix4x4 view;
		Matrix4x4 projection;
		Vector4 cameraPosition;
	};

	struct ObjectConstants
	{
		Matrix4x4 world;
		Matrix4x4 normalMatrix;
		Vector4 tint;
	};

	static_assert(sizeof(ViewConstants) == 144);
	static_assert(sizeof(ObjectConstants) == 144);
	static_assert(sizeof(DirectionalLight) == 48);

	ViewConstants viewConstants;
	viewConstants.view = view.view;
	viewConstants.projection = view.projection;
	viewConstants.cameraPosition.x = view.cameraPosition.x;
	viewConstants.cameraPosition.y = view.cameraPosition.y;
	viewConstants.cameraPosition.z = view.cameraPosition.z;
	viewConstants.cameraPosition.w = 0;

	ObjectConstants objectConstants;
	Result worldPrepared = DeriveObjectWorld(object, view, objectConstants.world);
	if (!worldPrepared.IsSuccess())
	{
		return worldPrepared;
	}

	// Normals must use the same derived World as positions.
	// Leaving the original normal matrix after Billboard rotation would produce inconsistent lighting.
	objectConstants.normalMatrix = objectConstants.world.Inverse().Transpose();
	objectConstants.tint = object.tint;

	DirectionalLight lightConstants;
	if (frame.directionalLight.has_value())
	{
		lightConstants = *frame.directionalLight;
	}

	std::array<float, 4> frameConstants{};
	frameConstants[0] = frame.time;
	Vector4 passConstants;
	passConstants.x = 1;
	passConstants.y = 1;
	passConstants.z = 1;
	passConstants.w = 1;

	const auto bytes = [](const auto& value)
	{
		using ConstantType = std::remove_cvref_t<decltype(value)>;
		const std::span<const ConstantType> values(&value, 1);
		return std::as_bytes(values);
	};

	std::vector<std::byte> materialConstants;
	UINT rootIndex = 0;
	for (const auto& buffer : inputs.pipeline->layout.constants)
	{
		std::span<const std::byte> constantBytes;
		switch (buffer.binding)
		{
		case 0:
			constantBytes = bytes(frameConstants);
			break;
		case 1:
			constantBytes = bytes(viewConstants);
			break;
		case 2:
			constantBytes = bytes(lightConstants);
			break;
		case 3:
			constantBytes = bytes(passConstants);
			break;
		case 4:
			constantBytes = bytes(objectConstants);
			break;
		case 5:
		{
			Result result = PackMaterial(buffer, inputs.material->desc, materialConstants);
			if (!result.IsSuccess())
			{
				return result;
			}
			constantBytes = materialConstants;
			break;
		}
		default:
			return { false, "PrepareConstantBindings: Unsupported constant buffer binding." };
		}

		backend::ConstantAllocation allocation;
		// Only frame/View/light/pass bindings may share one allocation inside this View preparation call.
		// Object and Material constants need their own ranges. The cache is recreated for the next View.
		const bool isCommonBuffer = buffer.binding < commonAllocations.size();
		if (isCommonBuffer && commonAllocations[buffer.binding].gpuAddress)
		{
			allocation = commonAllocations[buffer.binding];
		}
		else
		{
			Result result = backend.AllocateConstants(constantBytes, allocation);
			if (!result.IsSuccess())
			{
				return result;
			}
			if (isCommonBuffer)
			{
				commonAllocations[buffer.binding] = allocation;
			}
		}

		backend::ConstantBinding binding;
		binding.rootIndex = rootIndex;
		binding.gpuAddress = allocation.gpuAddress;
		command.m_packet.constants.push_back(binding);
		++rootIndex;
	}

	return {};
}

// Resolve the same owner that declaration imported, copy its CPU SRV to the frame-visible heap,
// and retain the native object while GPU commands may read it.
// Descriptor copying only creates another view description; Fence-based retention protects actual storage.
Result DrawResources::PrepareTextureBindings(
	const DrawInputs& inputs,
	FrameDrawContext& drawContext,
	DrawCommand& command)
{
	UINT rootIndex = static_cast<UINT>(command.m_packet.constants.size());
	for (const auto& requirement : inputs.pipeline->layout.resources)
	{
		if (requirement.sampler)
		{
			continue;
		}

		SampledTexture sampled;
		Result resolved;
		if (drawContext.m_renderTargets)
		{
			resolved = ResolveSampledTexture(inputs.material->desc, requirement,
				drawContext.m_textures, *drawContext.m_renderTargets, sampled);
		}
		else
		{
			resolved = ResolveTextureInput(inputs.material->desc, requirement,
				drawContext.m_textures, sampled.textureHandle, sampled.texture);
		}

		if (!resolved.IsSuccess())
		{
			return resolved;
		}

		graph::GraphTextureHandle graphTexture;
		D3D12_CPU_DESCRIPTOR_HANDLE sourceDescriptor{};
		ID3D12Resource* nativeResource = nullptr;
		std::uint64_t* drawFence = nullptr;
		if (sampled.target)
		{
			resolved = drawContext.m_bindings.FindImportedRenderTarget(*sampled.target, graphTexture);
			sourceDescriptor = sampled.target->gpuData->Srv();
			nativeResource = sampled.target->gpuData->Resource();
			drawFence = &sampled.target->lastDrawFenceValue;
		}
		else
		{
			resolved = drawContext.m_bindings.FindImportedTexture(*sampled.texture, graphTexture);
			sourceDescriptor = sampled.texture->gpuData->GetSrvHandle();
			nativeResource = sampled.texture->gpuData->texture.Get();
			drawFence = &sampled.texture->lastDrawFenceValue;
		}

		if (!resolved.IsSuccess())
		{
			return resolved;
		}

		D3D12_GPU_DESCRIPTOR_HANDLE descriptor{};
		Result copied = drawContext.m_backend.CopyFrameDescriptor(sourceDescriptor, descriptor);
		if (!copied.IsSuccess())
		{
			return copied;
		}

		backend::TextureBinding binding;
		binding.rootIndex = rootIndex;
		binding.descriptor = descriptor;
		command.m_packet.textures.push_back(binding);
		++rootIndex;

		graph::PassTextureUse read;
		read.texture = graphTexture;
		read.use.usage = graph::TextureUsage::ShaderRead;
		read.use.stages = static_cast<graph::ShaderStage>(static_cast<unsigned>(requirement.stages));
		read.requiresValidContents = true;
		command.m_reads.push_back(read);
		command.m_fences.push_back(drawFence);

		Result retained = drawContext.m_backend.KeepFrameObjectAlive(nativeResource);
		if (!retained.IsSuccess())
		{
			return retained;
		}
	}

	return {};
}

// Hold the native Pipeline/Root/Geometry objects independently of CPU registry borrows.
// Procedural input has no Geometry buffers to retain. Usage revisions are stamped after successful submission.
Result DrawResources::RetainDrawResources(
	const DrawInputs& inputs,
	backend::Backend& backend,
	DrawCommand& command)
{
	if (inputs.geometry)
	{
		command.m_fences.push_back(&inputs.geometry->lastDrawFenceValue);
	}
	command.m_fences.push_back(&inputs.material->lastDrawFenceValue);
	command.m_fences.push_back(&inputs.pipeline->lastDrawFenceValue);
	command.m_fences.push_back(&inputs.vertexShader->lastDrawFenceValue);
	if (inputs.pixelShader)
	{
		command.m_fences.push_back(&inputs.pixelShader->lastDrawFenceValue);
	}

	std::array<IUnknown*, 4> resources{};
	resources[0] = command.m_packet.pipeline;
	resources[1] = command.m_packet.root;
	if (inputs.geometry)
	{
		resources[2] = inputs.geometry->gpuData->vertices.GetNativeResource();
		resources[3] = inputs.geometry->gpuData->indices.GetNativeResource();
	}

	for (IUnknown* resource : resources)
	{
		if (!resource)
		{
			continue;
		}

		Result result = backend.KeepFrameObjectAlive(resource);
		if (!result.IsSuccess())
		{
			return result;
		}
	}

	return {};
}

// Prepare commands for one View's original Object indices and current output layout.
// Resolve effective viewport/scissor and remove empty clips before native recording.
// Allocation/preparation failure aborts the unsent Graph rather than submitting a partial frame.
Result DrawResources::PrepareCommands(
	FrameDrawContext& drawContext,
	const RenderView& view,
	const RenderList& objectIndices,
	const DrawSettings& settings,
	const graph::OutputLayout& output,
	std::vector<DrawCommand>& commands)
{
	// This cache is deliberately local to one PrepareCommands call for one View.
	// Moving it to frame-global state would reuse Camera A addresses when preparing Camera B.
	std::array<backend::ConstantAllocation, 4> commonAllocations{};
	for (std::size_t objectIndex : objectIndices)
	{
		if (objectIndex >= drawContext.m_frame.objects.size())
		{
			return { false, "PrepareCommands: Object index is out of range." };
		}

		DrawInputs inputs;
		DrawDiagnostic diagnostic;
		Result result = drawContext.ResolveObjectInputs(objectIndex, settings, inputs, diagnostic);
		if (!result.IsSuccess())
		{
			drawContext.AddDiagnostic(diagnostic, "DrawResolve", result.message);
			continue;
		}

		if (!drawContext.AreResourcesPrepared(inputs))
		{
			drawContext.AddDiagnostic(diagnostic, "DrawResolve", "Selected Resource is not ready.");
			continue;
		}

		const bool colorCountMismatch = inputs.pixelShader && inputs.pixelShader->metadata.colorOutputs != output.colorFormats.size();
		const bool missingPixelShader = !inputs.pixelShader && !output.colorFormats.empty();
		const bool missingDepth = inputs.pipeline->desc.depth != DepthMode::Disabled && !output.depthFormat;
		if (colorCountMismatch || missingPixelShader || missingDepth)
		{
			drawContext.AddDiagnostic(diagnostic, "DrawResolve", "Pipeline does not match the OutputLayout.");
			continue;
		}

		PipelineVariant* pipeline = nullptr;
		result = ResolvePipeline(inputs, diagnostic, output, drawContext.m_backend, pipeline);
		if (!result.IsSuccess())
		{
			return result;
		}

		DrawCommand command;
		command.m_packet.pipeline = pipeline->pipelineState.Get();
		command.m_packet.root = pipeline->rootSignature.Get();
		command.m_packet.heap = drawContext.m_backend.GetFrameDescriptorHeap();
		const auto& object = drawContext.m_frame.objects[objectIndex];
		if (object.procedural)
		{
			command.m_packet.requiresVertexBuffer = false;
			command.m_packet.count = object.procedural->vertexCount;
		}
		else
		{
			command.m_packet.vertices = inputs.geometry->gpuData->vertexView;
			command.m_packet.indices = inputs.geometry->gpuData->indexView;
			const bool indexed = inputs.geometry->gpuData->indexCount != 0;
			command.m_packet.kind = indexed ? backend::DrawKind::Indexed : backend::DrawKind::Vertices;
			const UINT availableCount = indexed ? inputs.geometry->gpuData->indexCount : inputs.geometry->gpuData->vertexCount;
			command.m_packet.first = object.drawRange.first;
			command.m_packet.baseVertex = object.drawRange.baseVertex;
			command.m_packet.count = object.drawRange.count == 0 ? availableCount - object.drawRange.first : object.drawRange.count;
		}

		// Resolve clipping before allocating constants/descriptors or retaining native objects.
		// An empty clipped draw records nothing and must not leak its scissor into the following draw.
		command.m_packet.scissor = DrawScissor(object, drawContext.m_activeViewport,
			drawContext.m_activeTargetWidth, drawContext.m_activeTargetHeight);
		if (command.m_packet.scissor.right <= command.m_packet.scissor.left ||
			command.m_packet.scissor.bottom <= command.m_packet.scissor.top)
		{
			continue;
		}
		result = PrepareConstantBindings(drawContext.m_frame, view, object, inputs, drawContext.m_backend, commonAllocations, command);
		if (!result.IsSuccess())
		{
			return result;
		}

		result = PrepareTextureBindings(inputs, drawContext, command);
		if (!result.IsSuccess())
		{
			return result;
		}

		result = RetainDrawResources(inputs, drawContext.m_backend, command);
		if (!result.IsSuccess())
		{
			return result;
		}
		commands.push_back(std::move(command));
	}

	return {};
}

Result DrawResources::ResolvePipeline(
	const DrawInputs& inputs,
	const DrawDiagnostic& diagnostic,
	const graph::OutputLayout& output,
	backend::Backend& backend,
	PipelineVariant*& result)
{
	auto& pipeline = *inputs.pipeline;

	if (output.colorFormats.size() > 1 || output.sampleCount != 1 || output.sampleQuality)
	{
		return { false, "ResolvePipeline: Multi-sample and multi-render target pipelines are not supported." };
	}

	if (pipeline.desc.depth != DepthMode::Disabled && !output.depthFormat)
	{
		return { false, "ResolvePipeline: Depth mode is enabled but no depth format is provided in the output layout." };
	}

	const bool colorCountMismatch = inputs.pixelShader && inputs.pixelShader->metadata.colorOutputs != output.colorFormats.size();
	const bool missingPixelShader = !inputs.pixelShader && !output.colorFormats.empty();
	if (colorCountMismatch || missingPixelShader)
	{
		return { false, "ResolvePipeline: Pixel shader color output count does not match the output layout." };
	}

	Pipelinekey key;
	key.pipeline = diagnostic.pipeline;
	key.vertex = pipeline.desc.vertexShader;
	key.pixel = pipeline.desc.pixelShader;
	key.vertexRevision = inputs.vertexShader->revision;
	key.pixelRevision = inputs.pixelShader ? inputs.pixelShader->revision : 0;
	key.vertexLayout = 1;
	key.colors = output.colorFormats;
	key.depth = output.depthFormat;
	key.sampleCount = output.sampleCount;
	key.sampleQuality = output.sampleQuality;

	for (auto& variant : pipeline.variants)
	{
		if (variant.key == key)
		{
			result = &variant;
			return {};
		}
	}

	PipelineVariant candidate;
	candidate.key = key;
	
	std::vector<D3D12_ROOT_PARAMETER> rootParameters;
	std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges;

	const auto textureCount = std::count_if(
		pipeline.layout.resources.begin(), pipeline.layout.resources.end(),
		[](const auto& x) 
		{
			return !x.sampler;	// Count only texture resources, not samplers.
		});

	descriptorRanges.reserve(textureCount);
	rootParameters.reserve(pipeline.layout.constants.size() + textureCount);

	for (const auto& cb: pipeline.layout.constants)
	{
		D3D12_ROOT_PARAMETER param{};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		param.Descriptor.ShaderRegister = cb.binding;
		param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters.push_back(param);
	}

	for (const auto& resource : pipeline.layout.resources)
	{
		if (resource.sampler)
		{
			continue;
		}

		D3D12_DESCRIPTOR_RANGE range{};
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		range.NumDescriptors = 1;
		range.BaseShaderRegister = resource.binding;
		range.RegisterSpace = 0;
		range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		descriptorRanges.push_back(range);

		D3D12_ROOT_PARAMETER param{};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param.DescriptorTable.NumDescriptorRanges = 1;
		param.DescriptorTable.pDescriptorRanges = &descriptorRanges.back();
		param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters.push_back(param);
	}

	std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;

	for (const auto& sampler : pipeline.layout.resources)
	{
		if (!sampler.sampler)
		{
			continue;
		}

		D3D12_STATIC_SAMPLER_DESC staticSampler{};
		staticSampler.ShaderRegister = sampler.binding;
		staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		
		auto addressMode = sampler.binding == 0 ? D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSampler.AddressU = addressMode;
		staticSampler.AddressV = addressMode;
		staticSampler.AddressW = addressMode;

		staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		staticSampler.MaxAnisotropy = 1;
		staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
		staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		staticSamplers.push_back(staticSampler);
	}

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.NumParameters = static_cast<UINT>(rootParameters.size());
	rootSigDesc.pParameters = rootParameters.data();
	rootSigDesc.NumStaticSamplers = static_cast<UINT>(staticSamplers.size());
	rootSigDesc.pStaticSamplers = staticSamplers.data();
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	auto checked = backend::CreateRootSignature(backend.GetNativeDevice(), rootSigDesc, candidate.rootSignature);

	if (!checked.IsSuccess())
	{
		return checked;
	}

	auto fields = VertexFields();
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;

	for (const auto& required: inputs.vertexShader->metadata.vertexInputs)
	{
		auto field = std::find_if(
			fields.begin(), fields.end(),
			[&](const VertexField& f)
			{
				return f.semantic == required.semantic;
			});

		D3D12_INPUT_ELEMENT_DESC element{};
		element.SemanticName = field->semantic;
		element.SemanticIndex = 0;
		element.Format = field->format;
		element.InputSlot = 0;
		element.AlignedByteOffset = field->offset;
		element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		element.InstanceDataStepRate = 0;

		inputElements.push_back(element);
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = candidate.rootSignature.Get();
	psoDesc.VS = { inputs.vertexShader->bytecode.data(), inputs.vertexShader->bytecode.size() };

	if (inputs.pixelShader)
	{
		psoDesc.PS = { inputs.pixelShader->bytecode.data(), inputs.pixelShader->bytecode.size() };
	}

	psoDesc.InputLayout = { inputElements.data(), static_cast<UINT>(inputElements.size()) };
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.SampleDesc.Count = output.sampleCount;
	psoDesc.SampleDesc.Quality = output.sampleQuality;
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

	auto cullMode = pipeline.desc.cull == CullMode::None
		? D3D12_CULL_MODE_NONE
		: (pipeline.desc.cull == CullMode::Back ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_FRONT);

	psoDesc.RasterizerState.CullMode = cullMode;
	psoDesc.RasterizerState.DepthClipEnable = TRUE;
	psoDesc.DepthStencilState.DepthEnable = pipeline.desc.depth != DepthMode::Disabled;
	
	auto depthWriteMask = pipeline.desc.depth == DepthMode::ReadWrite
		? D3D12_DEPTH_WRITE_MASK_ALL
		: D3D12_DEPTH_WRITE_MASK_ZERO;

	psoDesc.DepthStencilState.DepthWriteMask = depthWriteMask;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	psoDesc.DepthStencilState.FrontFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
	psoDesc.DepthStencilState.BackFace = psoDesc.DepthStencilState.FrontFace;

	auto& blendDesc = psoDesc.BlendState.RenderTarget[0];
	blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	
	auto srcBlend = pipeline.desc.blend == BlendMode::Alpha ? D3D12_BLEND_SRC_ALPHA : D3D12_BLEND_ONE;
	auto destBlend = pipeline.desc.blend == BlendMode::Alpha ? D3D12_BLEND_INV_SRC_ALPHA : D3D12_BLEND_ZERO;

	blendDesc.SrcBlend = srcBlend;
	blendDesc.DestBlend = destBlend;
	blendDesc.BlendEnable = pipeline.desc.blend == BlendMode::Alpha;
	blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;

	psoDesc.NumRenderTargets = static_cast<UINT>(output.colorFormats.size());

	for (std::size_t i = 0; i < output.colorFormats.size(); ++i)
	{
		psoDesc.RTVFormats[i] = NativeFormat(output.colorFormats[i]);
	}

	psoDesc.DSVFormat = output.depthFormat
		? NativeFormat(*output.depthFormat)
		: DXGI_FORMAT_UNKNOWN;

	auto created = backend::CreateGraphicsPipeline(
		backend.GetNativeDevice(),
		psoDesc,
		candidate.pipelineState);

	if (!created.IsSuccess())
	{
		return created;
	}

	pipeline.variants.push_back(std::move(candidate));
	result = &pipeline.variants.back();

	return {};
}

}

namespace rendering::graph
{

Result RenderPassContext::RecordDrawCommands(std::span<const rendering::DrawCommand> commands)
{
	if (!m_status.IsSuccess())
	{
		return m_status;
	}

	if (!m_commands || !m_drawFences)
	{
		return Reject("RecordDrawCommands: Context has no Draw recorder");
	}

	for (const auto& command : commands)
	{
		for (const auto& read : command.m_reads)
		{
			PassTextureInfo info;
			auto checked = GetTextureInfo(read.texture, read.use, info);

			if (!checked.IsSuccess())
			{
				return checked;
			}
		}

		for (auto* fence : command.m_fences)
		{
			if (std::find(m_drawFences->begin(), m_drawFences->end(), fence) == m_drawFences->end())
			{
				m_drawFences->push_back(fence);
			}
		}

		auto recorded = m_commands->RecordDraw(command.m_packet);

		if (!recorded.IsSuccess())
		{
			m_status = recorded;
			return m_status;
		}
	}

	return {};
}

}