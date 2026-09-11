#include "Engine/UI/UIImage.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/UI/Canvas.h"

bool UIImage::SetTextureAsset(const Guid& assetId)
{
	if (!assetId.IsValid())
	{
		m_textureAssetId = Guid{};
		m_pendingTextureAssetId.reset();
		m_renderTemplate.clear();
		m_isProxyDirty = true;

		return true;
	}

	// Get the engine context to access the asset and texture managers
	EngineContext* context = GetEngineContext();
	if (!context || !context->pAssetManager || !context->pTextureManager)
	{
		return false;
	}

	// Get the asset entry for the given asset ID and check if it's a valid texture asset
	const AssetEntry* assetEntry = context->pAssetManager->GetAssetEntry(assetId);
	if (!assetEntry || assetEntry->type != AssetType::Texture)
	{
		return false;
	}

	// Get the texture handle for the asset ID and check if it's valid
	const TextureHandle textureHandle = context->pAssetManager->GetTextureHandle(assetId);
	if (textureHandle == InvalidTextureHandle)
	{
		return false;
	}

	// Build the render template with a single UIRenderElement using the texture handle
	UIRenderElement element;
	element.materialDesc.textureHandle = textureHandle;
	element.materialDesc.psoKey = PSO_KEY_DEFAULT::UI;
	element.materialDesc.baseColor = { 1, 1, 1, 1 };
	element.materialDesc.lightingEnabled = false;

	// Set the required member variables and mark the proxy as dirty
	m_renderTemplate = { element };
	m_textureAssetId = assetId;
	m_pendingTextureAssetId.reset();
	m_isProxyDirty = true;

	return true;
}


bool UIImage::ResolveReferences(SceneBase& scene)
{
	Canvas* resolvedCanvas = GetGoverningCanvas();
	bool textureCanBeResolved = false;

	// Resolve the canvas actor if a pending canvas actor ID is set
	if (m_pendingCanvasActorId.has_value())
	{
		Actor* canvasActor = scene.ResolveActor(*m_pendingCanvasActorId);

		if (!canvasActor) return false;
	
		resolvedCanvas = canvasActor->GetComponentByClass<Canvas>();
		
		if (!resolvedCanvas) return false;
	}

	// Resolve the texture asset if a pending texture asset ID is set
	if (m_pendingTextureAssetId.has_value())
	{
		Actor* owner = GetOwner();

		if (!owner || owner->GetOwner() != &scene) return false;

		EngineContext* context = owner->GetOwner()->GetEngineContext();
		if (!context || !context->pAssetManager || !context->pTextureManager) return false;

		const AssetEntry* assetEntry = context->pAssetManager->GetAssetEntry(*m_pendingTextureAssetId);
		if (!assetEntry || assetEntry->type != AssetType::Texture)
		{
			m_renderTemplate.clear();
			m_textureAssetId = {};
			m_isProxyDirty = true;
		}
		else
		{
			textureCanBeResolved = true;
		}
	}

	// Set the resolved texture asset
	if (textureCanBeResolved)
	{
		if (!SetTextureAsset(*m_pendingTextureAssetId)) return false;
	}

	// Set the resolved canvas
	if (m_pendingCanvasActorId.has_value())
	{
		SetCanvas(resolvedCanvas);
		m_pendingCanvasActorId.reset();
	}

	m_isProxyDirty = true;

	return true;
}
