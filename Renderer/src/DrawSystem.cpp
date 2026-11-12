// Resource registration, input validation and Pipeline variant caching.
// Command preparation lives in DrawCommandPreparation.cpp; native PSO creation belongs to Backend.

#include "DrawSystem.h"
#include "Backend/PipelineGpu.h"
#include "Math/Finite.h"
#include "Failure.h"

#include <algorithm>
#include <cmath>
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
	if (!Finite(light.lightDirection) || !Finite(light.lightColor) || !Finite(light.ambient))
	{
		return {false, "ValidateLight: non-finite parameters."};
	}

	const auto& direction = light.lightDirection;

	const float lengthSquared = direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2];

	if (lengthSquared < 1e-12f)
	{
		return {false, "ValidateLight: direction is too small."};
	}

	return {};
}

Result ValidatePipelineDesc(const PipelineDesc& desc)
{
	if (desc.blend != BlendMode::Opaque && desc.blend != BlendMode::Alpha)
	{
		return {false, "ValidatePipelineDesc: unsupported blend mode."};
	}

	if (desc.depth != DepthMode::Disabled && desc.depth != DepthMode::ReadOnly && desc.depth != DepthMode::ReadWrite)
	{
		return {false, "ValidatePipelineDesc: unsupported depth mode."};
	}

	if (desc.cull != CullMode::None && desc.cull != CullMode::Back && desc.cull != CullMode::Front)
	{
		return {false, "ValidatePipelineDesc: unsupported cull mode."};
	}

	if (desc.fill != FillMode::Solid && desc.fill != FillMode::Wireframe)
	{
		return Fail("ValidatePipelineDesc: unsupported fill mode.");
	}

	if (desc.blend == BlendMode::Alpha && desc.depth == DepthMode::ReadWrite)
	{
		return {false, "ValidatePipelineDesc: Transparent Pipeline must disable Depth writes."};
	}

	return {};
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
		return {false, "ValidateShaderLink: Vertex shader reflection failed."};
	}

	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> pixelReflection;
	auto psHr = D3DReflect(pixel->bytecode.data(), pixel->bytecode.size(), IID_PPV_ARGS(&pixelReflection));

	if (FAILED(psHr))
	{
		return {false, "ValidateShaderLink: Pixel shader reflection failed."};
	}

	// Retrieve the shader descriptions for both vertex and pixel shaders.
	D3D12_SHADER_DESC vertexDesc{};

	if (FAILED(vertexReflection->GetDesc(&vertexDesc)))
	{
		return {false, "ValidateShaderLink: Vertex shader reflection description failed."};
	}

	D3D12_SHADER_DESC pixelDesc{};

	if (FAILED(pixelReflection->GetDesc(&pixelDesc)))
	{
		return {false, "ValidateShaderLink: Pixel shader reflection description failed."};
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
			return {false, "ValidateShaderLink: Pixel shader linkage lookup failed"};
		}

		if (!input.ReadWriteMask ||
			(input.SystemValueType != D3D_NAME_UNDEFINED && input.SystemValueType != D3D_NAME_POSITION))
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
				return {false, "ValidateShaderLink: Vertex shader linkage lookup failed"};
			}

			// Check if the semantic names, indices, component types, and read/write masks
			// match between the vertex shader output and pixel shader input.
			if (canonical(input.SemanticName) == canonical(output.SemanticName) &&
				input.SemanticIndex == output.SemanticIndex && input.ComponentType == output.ComponentType &&
				(input.ReadWriteMask & ~output.Mask) == 0)
			{
				match = true;
			}
		}

		if (!match)
		{
			return {false, "ValidateShaderLink: VS/PS semantic mismatch: " + std::string(input.SemanticName)};
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

} // namespace

Result ValidateFrameInputs(const FrameRenderData& data)
{
	if (!std::isfinite(data.time))
	{
		return Fail("Time must be a finite value.");
	}

	for (const Matrix4x4 & matrix : data.skinningPalettes)
	{
		if (!Finite(matrix))
		{
			return Fail("Skinning palettes must contain finite matrices.");
		}
	}

	for (const RenderObject& object : data.objects)
	{
		if (const auto& range = object.skinningPalette)
		{
			if (range->begin >= range->end || range->end > data.skinningPalettes.size())
			{
				return Fail("RenderObject has an invalid skinning palette range.");
			}
		}
	}

	if (data.directionalLight.has_value())
	{
		auto checked = ValidateLight(data.directionalLight.value());

		if (!checked.IsSuccess())
		{
			return {false, "ValidateFrameInputs: invalid Frame light input"};
		}
	}

	for (const auto& view : data.views)
	{
		if (!std::isfinite(view.exposure) || view.exposure < 0)
		{
			return Fail("Exposure must be a finite nonnegative multiplier.");
		}

		if (!Finite(view.view) || !Finite(view.projection) ||
			!Finite(Vector4{view.cameraPosition.x, view.cameraPosition.y, view.cameraPosition.z, 0}))
		{
			return {false, "ValidateFrameInputs: invalid Frame view input"};
		}
	}

	return {};
}

ResourceRegistrationResult<GeometryHandle> DrawResources::Register(const GeometryDesc& desc)
{
	return m_geometryResources.Register(desc);
}

ResourceRegistrationResult<TextureHandle> DrawResources::Register(const TextureDesc& desc)
{
	return m_textureResources.Register(desc);
}

ResourceRegistrationResult<ShaderHandle> DrawResources::Register(const ShaderDesc& desc)
{
	auto candidate = std::make_unique<ShaderContent>();

	auto result = ReflectShader(desc, *candidate);

	if (!result.IsSuccess())
	{
		return {result, {}};
	}

	return m_shaderResources.Register(std::move(candidate));
}

ResourceRegistrationResult<PipelineHandle> DrawResources::Register(const PipelineDesc& desc)
{
	auto checked = ValidatePipelineDesc(desc);

	if (!checked.IsSuccess())
	{
		return {checked, {}};
	}

	auto* vs = m_shaderResources.FindForNewUse(desc.vertexShader);
	auto* ps = m_shaderResources.FindForNewUse(desc.pixelShader);

	if (!vs || vs->metadata.stage != ShaderStage::Vertex)
	{
		return {{false, "RegisterPipeline: Vertex shader handle is invalid or not a vertex shader."}, {}};
	}

	if (desc.pixelShader != ShaderHandle{} && (!ps || ps->metadata.stage != ShaderStage::Pixel))
	{
		return {{false, "RegisterPipeline: Pixel shader handle is invalid or not a pixel shader."}, {}};
	}

	checked = ValidateShaderLink(*vs, ps);

	if (!checked.IsSuccess())
	{
		return {checked, {}};
	}

	auto candidate = std::make_unique<PipelineContent>();

	candidate->desc = desc;

	checked = MergeMetadata(vs->metadata, ps ? &ps->metadata : nullptr, candidate->layout);

	if (!checked.IsSuccess())
	{
		return {checked, {}};
	}

	auto fields = backend::VertexFields();

	for (const auto& input : vs->metadata.vertexInputs)
	{
		bool mismatchVertex = std::none_of(
				fields.begin(), fields.end(),
				[&](const backend::VertexField& field)
				{
					return input.semantic == field.semantic && input.components == field.count;
				});

		if (input.index != 0 || mismatchVertex)
		{
			return {{false, "RegisterPipeline: MeshVertex input mismatch: " + input.semantic}, {}};
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
		return {{false, "RegisterPipeline: Root Signature exceeds 64 DWORDs"}, {}};
	}

	return m_pipelineResources.Register(std::move(candidate));
}

ResourceRegistrationResult<MaterialHandle> DrawResources::Register(const MaterialDesc& desc)
{
	auto checked = ValidateMaterial(desc);

	if (!checked.IsSuccess())
	{
		return {checked, {}};
	}

	if (!m_pipelineResources.FindForNewUse(desc.pipeline))
	{
		return {{false, "RegisterMaterial: Pipeline handle is invalid."}, {}};
	}

	auto candidate = std::make_unique<MaterialContent>();
	candidate->desc = desc;

	return m_materialResources.Register(std::move(candidate));
}

Result DrawResources::UpdateMaterial(MaterialHandle handle, const MaterialDesc& desc)
{
	auto checked = ValidateMaterial(desc);

	if (!checked.IsSuccess())
	{
		return checked;
	}

	if (!m_pipelineResources.FindForNewUse(desc.pipeline))
	{
		return {false, "UpdateMaterial: Pipeline handle is invalid."};
	}

	auto candidate = std::make_unique<MaterialContent>();
	candidate->desc = desc;

	return m_materialResources.Update(handle, std::move(candidate));
}

Result DrawResources::ResolveTextureInput(
	const MaterialDesc& material, const ShaderResourceBinding& requirement,
	TextureHandle& handle, TextureContent*& content)
{
	const MaterialParameter* parameter = FindMaterialParameter(material, requirement.name);

	if (!parameter)
	{
		return {false, "Missing Texture2D parameter: " + requirement.name};
	}

	if (!std::holds_alternative<TextureHandle>(parameter->value))
	{
		return {false, "Expected Texture2D parameter: " + requirement.name};
	}

	const TextureHandle selectedHandle = std::get<TextureHandle>(parameter->value);
	// Invalid identity remains invalid. Only this draw's sampling source changes;
	// the caller's Material and registration table retain their original values.
	handle = selectedHandle;

	if (!m_textureResources.FindForNewUse(selectedHandle))
	{
		handle = fallbackTexture;
	}

	content = m_textureResources.FindRegisteredContent(handle);

	if (!content || !m_textureResources.FindForNewUse(handle))
	{
		return Fail("Texture and resident substitute unavailable: " + requirement.name);
	}

	return {};
}

// Use the Material value's handle type to select the correct Registry owner.
// This operation creates no read-only alias handle, descriptor ownership or duplicated GPU image.
Result DrawResources::ResolveSampledTexture(
	const MaterialDesc& material,
	const ShaderResourceBinding& requirement,
	SampledTexture& sampled)
{
	const MaterialParameter* parameter = FindMaterialParameter(material, requirement.name);

	if (!parameter)
	{
		return Fail("Missing Texture2D parameter: " + requirement.name);
	}

	if (const auto* handle = std::get_if<RenderTargetTextureHandle>(&parameter->value))
	{
		RenderTargetInput target;
		target.handle = *handle;
		target.content = m_renderTargetResources.FindForNewUse(*handle);

		if (target.content && target.content->gpuData)
		{
			sampled = target;
			return {};
		}

		UploadedTextureInput fallback;
		fallback.handle = fallbackTexture;
		fallback.content = m_textureResources.FindRegisteredContent(fallbackTexture);

		if (!fallback.content || !m_textureResources.FindForNewUse(fallbackTexture))
		{
			return Fail("RenderTargetTexture and resident substitute unavailable: " + requirement.name);
		}

		sampled = fallback;
		return {};
	}

	UploadedTextureInput texture;
	Result resolved = ResolveTextureInput(material, requirement, texture.handle, texture.content);

	if (resolved.IsSuccess())
	{
		sampled = texture;
	}

	return resolved;
}

// Resolve the final Pipeline after applying View overrides, then validate its complete input contract.
// Vertex deformation and Texture Alpha inputs must remain available in a compatible override.
// This strict leaf resolver never changes caller identities. FrameDrawContext
// may retry with a scoped substitute after retaining the original failure cause.
Result DrawResources::ResolveDrawInputs(
	const RenderObject& object, std::size_t objectIndex,
	const DrawSettings& settings, DrawInputs& inputs, DrawDiagnostic& diagnostic)
{
	diagnostic.objectIndex = objectIndex;
	diagnostic.geometry = object.geometry;
	diagnostic.material = settings.materialOverride.value_or(object.material);

	DrawInputs selected;
	selected.depthOverride = settings.depthOverride;

	if (selected.depthOverride)
	{
		const DepthMode mode = *selected.depthOverride;
		const bool supported =
			mode == DepthMode::Disabled || mode == DepthMode::ReadOnly || mode == DepthMode::ReadWrite;

		if (!supported)
		{
			return Fail("Unsupported draw Depth override.");
		}
	}

	if (!object.procedural)
	{
		if (!m_geometryResources.FindForNewUse(object.geometry))
		{
			return Fail("Geometry unavailable: " + m_geometryResources.DescribeUnavailable(object.geometry));
		}

		selected.geometry = m_geometryResources.FindRegisteredContent(object.geometry);
		diagnostic.geometryRevision = selected.geometry->revision;
	}

	selected.material = m_materialResources.FindForNewUse(diagnostic.material);

	if (!selected.material)
	{
		return {false, "Material unavailable: " + m_materialResources.DescribeUnavailable(diagnostic.material)};
	}

	diagnostic.materialRevision = selected.material->revision;

	diagnostic.pipeline = settings.pipelineOverride.value_or(selected.material->desc.pipeline);
	selected.pipeline = m_pipelineResources.FindForNewUse(diagnostic.pipeline);

	if (!selected.pipeline)
	{
		return {false, "Pipeline unavailable: " + m_pipelineResources.DescribeUnavailable(diagnostic.pipeline)};
	}

	const DepthMode effectiveDepth = settings.depthOverride.value_or(selected.pipeline->desc.depth);

	if (selected.pipeline->desc.blend == BlendMode::Alpha && effectiveDepth == DepthMode::ReadWrite)
	{
		return Fail("Alpha draws cannot write Depth, including View overrides.");
	}

	diagnostic.vertexShader = selected.pipeline->desc.vertexShader;
	diagnostic.pixelShader = selected.pipeline->desc.pixelShader;
	selected.vertexShader = m_shaderResources.FindForNewUse(diagnostic.vertexShader);
	selected.pixelShader = m_shaderResources.FindForNewUse(diagnostic.pixelShader);
	const bool requiredPixelMissing = diagnostic.pixelShader != ShaderHandle{} && !selected.pixelShader;

	if (!selected.vertexShader || requiredPixelMissing)
	{
		return {false, "Dependent Shader unavailable: VS " + m_shaderResources.DescribeUnavailable(diagnostic.vertexShader) +
                           "; PS " + m_shaderResources.DescribeUnavailable(diagnostic.pixelShader)};
	}

	diagnostic.vertexRevision = selected.vertexShader->revision;
	diagnostic.pixelRevision = selected.pixelShader ? selected.pixelShader->revision : 0;

	if (!Finite(object.world) || !Finite(object.tint))
	{
		return {false, "World and Tint must be finite."};
	}

	const float worldDeterminant = object.world.Determinant();

	if (!std::isfinite(worldDeterminant) || std::abs(worldDeterminant) < 1e-12f)
	{
		return {false, "World must be invertible."};
	}

	const bool supportedBillboard = object.billboard == BillboardMode::None ||
									object.billboard == BillboardMode::Spherical ||
									object.billboard == BillboardMode::Cylindrical;

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

			const MaterialParameter* parameter = FindMaterialParameter(selected.material->desc, variable.name);

			if (!parameter)
			{
				return {false, "Missing Material parameter: " + variable.name};
			}

			if (TypeOf(parameter->value) != variable.type)
			{
				return {false, "Material type mismatch: " + variable.name};
			}
		}
	}

	for (const auto& requirement : selected.pipeline->layout.resources)
	{
		// Reserved View-common data is supplied by FrameDrawContext, never a
		// Material parameter. Reflection already verified its exclusive slot.
		if (requirement.name == "RendererShadowMap")
		{
			continue;
		}

		if (requirement.sampler)
		{
			continue;
		}

		Result resolved;
		SampledTexture sampled;
		resolved = ResolveSampledTexture(selected.material->desc, requirement,sampled);

		if (!resolved.IsSuccess())
		{
			return resolved;
		}
	}

	inputs = selected;
	return {};
}

Result DrawResources::GetOrCreatePipelineVariant(
	const DrawInputs& inputs, const DrawDiagnostic& diagnostic,
	const graph::OutputLayout& output, backend::Backend& backend,
	PipelineVariant*& result)
{
	auto& pipeline = *inputs.pipeline;
	const DepthMode effectiveDepth = inputs.depthOverride.value_or(pipeline.desc.depth);

	if (output.colorFormats.size() > 1 || output.sampleCount != 1 || output.sampleQuality)
	{
		return {false, "GetOrCreatePipelineVariant: Multi-sample and multi-render target pipelines are not supported."};
	}

	if (effectiveDepth != DepthMode::Disabled && !output.depthFormat)
	{
		return {
			false,
			"GetOrCreatePipelineVariant: Depth mode is enabled but no depth format is provided in the output layout."};
	}

	const bool colorCountMismatch =
		inputs.pixelShader && inputs.pixelShader->metadata.colorOutputs != output.colorFormats.size();
	const bool missingPixelShader = !inputs.pixelShader && !output.colorFormats.empty();

	if (colorCountMismatch || missingPixelShader)
	{
		return {false, "GetOrCreatePipelineVariant: Pixel shader color output count does not match the output layout."};
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
	key.effectiveDepth = effectiveDepth;
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

	backend::PipelineBuildInputs build;
	build.description = &pipeline.desc;
	build.layout = &pipeline.layout;
	build.vertex = inputs.vertexShader;
	build.pixel = inputs.pixelShader;
	build.output = &output;
	build.depth = effectiveDepth;
	Result created = backend::BuildPipeline(backend.GetNativeDevice(), build, candidate.gpu);

	if (!created.IsSuccess())
	{
		return created;
	}

	pipeline.variants.push_back(std::move(candidate));
	result = &pipeline.variants.back();

	return {};
}

} // namespace rendering::detail
