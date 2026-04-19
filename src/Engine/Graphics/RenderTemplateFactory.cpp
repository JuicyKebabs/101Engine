#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Model/AssimpLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

MeshRenderTemplate RenderTemplateFactory::CreateMeshRenderTemplate(MeshManager& meshManager, TextureManager& textureManager, const MeshInput& meshInput, const MaterialInput& materialInput)
{
	Model model = LoadModelFromFile(meshInput.modelPath, meshInput.inverseU, meshInput.inverseV);
	MaterialDesc materialDesc = BuildMaterialDesc(textureManager, materialInput);
	return BuildRenderTemplate(meshManager, model, materialDesc);

}

MeshRenderTemplate RenderTemplateFactory::CreateMeshRenderTemplateFromDefaultMesh(MeshManager& meshManager, TextureManager& textureManager, DEFAULT_MESH mesh, const MaterialInput& materialInput)
{
	Model model = LoadDefaultModel(mesh);
	MaterialDesc materialDesc = BuildMaterialDesc(textureManager, materialInput);
	return BuildRenderTemplate(meshManager, model, materialDesc);
}

SpriteRenderTemplate RenderTemplateFactory::CreateSpriteRenderTemplate(TextureManager& textureManager, const MaterialInput& materialInput, BillboardType billboardType)
{
	SpriteRenderTemplate temp;
	MaterialDesc materialDesc = BuildMaterialDesc(textureManager, materialInput);
	temp.materialDesc = materialDesc;
	temp.billboardType = billboardType;
	return temp;
}

MaterialDesc RenderTemplateFactory::BuildMaterialDesc(TextureManager& textureManager, const MaterialInput& input)
{
	MaterialDesc desc;
	desc.textureHandle = textureManager.LoadTexture(input.texturePath);
	desc.baseColor = input.baseColor;
	desc.psoKey = input.psoKey;
	desc.lightingEnabled = input.lightingEnabled;
	return desc;
}

Model RenderTemplateFactory::LoadModelFromFile(const std::wstring& path, bool inverseU, bool inverseV)
{
	Model model;

	if (!path.empty()) {
		ImportSettings settings = {
			path.c_str(),	// File path
			model,			// Reference to the model data vector
			inverseU,		// Whether to invert U coordinate
			inverseV		// Whether to invert V coordinate
		};

		AssimpLoader::Load(settings);
	}

	return model;
}

Model RenderTemplateFactory::LoadDefaultModel(DEFAULT_MESH type)
{
	Model model = GetDefaultModel(type);
	return model;
}

MeshRenderTemplate RenderTemplateFactory::BuildRenderTemplate(MeshManager& meshManager, Model& model, MaterialDesc& materialDesc)
{
	MeshRenderTemplate temp;

	// Create a render template for each mesh in the model
	for (auto& mesh : model) {
		SubmeshRenderTemplate sub;
		sub.meshDesc.gpuHandle = meshManager.CreateMesh(mesh);
		sub.meshDesc.baseVertex = 0;
		sub.meshDesc.startIndex = 0;
		sub.meshDesc.boundsCenter = mesh.boundsCenter;
		sub.meshDesc.boundsRadius = mesh.boundsRadius;
		sub.materialDesc = materialDesc;
		temp.push_back(sub);
	}

	return temp;
}
