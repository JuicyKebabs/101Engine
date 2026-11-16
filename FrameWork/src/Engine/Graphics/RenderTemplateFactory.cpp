#include "RenderTemplateFactory.h"

#include "Engine/Core/Context/Context.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/RenderAssetResources.h"
#include "Renderer/Renderer.h"

SubmeshRenderTemplate MakeSubmeshRenderTemplate(
	const EngineContext& context,
	const GeometryInfo& geometry,
	bool screen)
{
	SubmeshRenderTemplate renderTemplate;
	renderTemplate.meshDesc.meshHandle = geometry.geometry;
	renderTemplate.localToModel = geometry.localToModel;
	renderTemplate.materialDesc.textureHandle = geometry.texture;
	renderTemplate.materialDesc.screen = screen;

	if (context.pRenderAssets)
	{
		Vector2 uvScale = Vector2::One();
		Vector2 uvOffset = Vector2::Zero();
		Vector2 pivot = Vector2(0.5f, 0.5f);

		renderTemplate.materialDesc.description =
			context.pRenderAssets->MakeMaterial(
				true,
				renderTemplate.materialDesc.screen,
				geometry.texture,
				uvScale,
				uvOffset,
				pivot,
				false,
				false);
	}

	if (context.pRenderer)
	{
		rendering::GeometryData snapshot;
		const auto queried = context.pRenderer->GetGeometryData(geometry.geometry, snapshot);

		if (queried.IsSuccess())
		{
			renderTemplate.meshDesc.boundsCenter = snapshot.boundsCenter;
			renderTemplate.meshDesc.boundsRadius = snapshot.boundsRadius;
			renderTemplate.meshDesc.boundsRevision = snapshot.revision;
		}
	}

	return renderTemplate;
}

SubmeshRenderTemplate PrepareSubmeshRenderTemplate(
	const EngineContext& context,
	SceneBase& scene,
	const GeometryInfo& geometry,
	bool screen)
{
	auto renderTemplate = MakeSubmeshRenderTemplate(context, geometry, screen);

	if (context.pRenderAssets)
	{
		renderTemplate.materialDesc.handle =
			context.pAssetManager->RegisterSceneMaterial(
				scene,
				renderTemplate.materialDesc.description);
	}

	return renderTemplate;
}