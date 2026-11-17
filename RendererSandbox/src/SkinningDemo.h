#pragma once

#include "Renderer/Renderer.h"

#include <array>
#include <filesystem>

namespace sandbox
{

	enum class SkinningPose
	{
		Bind,
		Translation,
		Blend
	};

	struct SkinningDemo
	{
		rendering::GeometryHandle geometry;
		std::array<rendering::BufferHandle, 2> palettes{};
		std::array<rendering::MaterialHandle, 2> materials{};
	};

	inline rendering::Result UpdateSkinningPose(rendering::Renderer& renderer,
		rendering::BufferHandle palette, SkinningPose pose)
	{
		using namespace rendering;
		std::array matrices{ Matrix4x4::Identity(), Matrix4x4::Identity() };

		if (pose == SkinningPose::Translation)
		{
			const Vector3 translation{ 0.0f, 0.5f, 0.0f };
			matrices[0] = Matrix4x4::CreateTranslation(translation);
		}
		else if (pose == SkinningPose::Blend)
		{
			const Vector3 translation{ 0.0f, 0.45f, 0.0f };
			matrices[1] = Matrix4x4::CreateRotationX(0.85f) * Matrix4x4::CreateTranslation(translation);
		}

		static_assert(sizeof(Matrix4x4) == 64);
		return renderer.UpdateStructuredBuffer(palette, std::as_bytes(std::span(matrices)));
	}

	inline rendering::Result PrepareSkinningDemo(rendering::Renderer& renderer,
		rendering::ShaderHandle vertexShader, rendering::ShaderHandle pixelShader, SkinningDemo& demo)
	{
		using namespace rendering;

		std::array<MeshVertex, 4> vertices{};
		vertices[0].position = Vector3{ -0.32f, -0.25f, 0.0f };
		vertices[1].position = Vector3{ -0.32f, 0.25f, 0.0f };
		vertices[2].position = Vector3{ 0.32f, 0.25f, 0.0f };
		vertices[3].position = Vector3{ 0.32f, -0.25f, 0.0f };

		for (auto& vertex : vertices)
		{
			vertex.normal.z = 1.0f;
		}

		std::array<SkinVertex, 4> skinVertices{};
		skinVertices[0].boneWeights[0] = 1.0f;
		skinVertices[1].boneWeights[0] = 1.0f;

		for (std::size_t index = 2; index < skinVertices.size(); ++index)
		{
			skinVertices[index].boneIndices[1] = 1;
			skinVertices[index].boneWeights[0] = 0.5f;
			skinVertices[index].boneWeights[1] = 0.5f;
		}

		const std::array<std::uint32_t, 6> indices{ 0, 1, 2, 0, 2, 3 };
		GeometryDesc geometryDesc;
		geometryDesc.vertices = vertices;
		geometryDesc.indices = indices;
		geometryDesc.skinVertices = skinVertices;
		const auto geometry = renderer.RegisterGeometry(geometryDesc);
		if (!geometry.result.IsSuccess())
		{
			return geometry.result;
		}

		demo.geometry = geometry.handle;
		PipelineDesc pipelineDesc;
		pipelineDesc.vertexShader = vertexShader;
		pipelineDesc.pixelShader = pixelShader;
		pipelineDesc.depth = DepthMode::ReadWrite;
		pipelineDesc.cull = CullMode::None;
		const auto pipeline = renderer.RegisterPipeline(pipelineDesc);
		if (!pipeline.result.IsSuccess())
		{
			return pipeline.result;
		}

		const std::array pixels{ std::byte{ 255 }, std::byte{ 255 }, std::byte{ 255 }, std::byte{ 255 } };
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

		StructuredBufferDesc paletteDesc;
		paletteDesc.elementCount = 2;
		paletteDesc.elementStride = sizeof(Matrix4x4);

		for (std::size_t index = 0; index < demo.palettes.size(); ++index)
		{
			const auto palette = renderer.CreateStructuredBuffer(paletteDesc);
			if (!palette.result.IsSuccess())
			{
				return palette.result;
			}

			Result initial = UpdateSkinningPose(renderer, palette.handle, SkinningPose::Bind);
			if (!initial.IsSuccess())
			{
				return initial;
			}

			MaterialDesc materialDesc;
			materialDesc.pipeline = pipeline.handle;

			MaterialParameter baseColor;
			baseColor.name = "BaseColor";
			baseColor.value = Vector4{ 0.25f, 0.75f, 0.5f, 1.0f };
			materialDesc.parameters.push_back(baseColor);

			MaterialParameter uvTransform;
			uvTransform.name = "UVTransform";
			uvTransform.value = Vector4{ 1.0f, 1.0f, 0.0f, 0.0f };
			materialDesc.parameters.push_back(uvTransform);

			MaterialParameter localQuadTransform;
			localQuadTransform.name = "LocalQuadTransform";
			localQuadTransform.value = Vector4{ 1.0f, 1.0f, 0.0f, 0.0f };
			materialDesc.parameters.push_back(localQuadTransform);

			MaterialParameter baseTexture;
			baseTexture.name = "BaseTexture";
			baseTexture.value = texture.handle;
			materialDesc.parameters.push_back(baseTexture);

			MaterialParameter boneMatrices;
			boneMatrices.name = "gBoneMatrices";
			boneMatrices.value = palette.handle;
			materialDesc.parameters.push_back(boneMatrices);

			const auto material = renderer.RegisterMaterial(materialDesc);
			if (!material.result.IsSuccess())
			{
				return material.result;
			}

			demo.palettes[index] = palette.handle;
			demo.materials[index] = material.handle;
		}

		return {};
	}

	inline void AppendSkinningDemo(rendering::FrameRenderData& frame, const SkinningDemo& demo)
	{
		using namespace rendering;
		DirectionalLight light;
		light.lightDirection = { 0.0f, -0.6f, -0.8f, 0.0f };
		frame.directionalLight = light;

		constexpr std::array centers{ -0.48f, 0.48f };
		const Vector3 scale{ 0.8f, 0.8f, 0.8f };

		for (std::size_t index = 0; index < demo.materials.size(); ++index)
		{
			const Vector3 placement{ centers[index], 0.0f, 0.5f };
			RenderObject object;
			object.geometry = demo.geometry;
			object.material = demo.materials[index];
			object.world = Matrix4x4::CreateScale(scale) * Matrix4x4::CreateTranslation(placement);
			frame.objects.push_back(object);
		}
	}

}