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

bool UIImage::Serialize(nlohmann::json& outJson) const
{
	if (!UIRenderer::Serialize(outJson)) return false;

	// Serialize the texture asset ID or pending texture asset ID if they are valid
	if (m_textureAssetId.IsValid())
	{
		outJson["textureAssetId"] = m_textureAssetId.ToString();
	}
	else if (m_pendingTextureAssetId.has_value())
	{
		outJson["textureAssetId"] = m_pendingTextureAssetId->ToString();
	}
	else
	{
		outJson["textureAssetId"] = nullptr;
	}

	return true;
}

bool UIImage::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object() || !json.contains("textureAssetId")) return false;

	std::optional<Guid> parsedTextureAssetId;

	const auto& textureJson = json["textureAssetId"];

	if (textureJson.is_null())
	{
		parsedTextureAssetId.reset();
	}
	else 
	{
		if (!textureJson.is_string()) return false;

		Guid parsedGuid;

		// Try to parse the texture asset ID from the JSON string
		try
		{
			if (!Guid::TryParse(
				textureJson.get<std::string>(),
				parsedGuid) || !parsedGuid.IsValid())
			{
				return false;
			}
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}

		parsedTextureAssetId = parsedGuid;
	}

	// Deserialize the base UIRenderer properties
	if (!UIRenderer::Deserialize(json)) return false;

	// Set the required member variables and mark the proxy as dirty
	m_renderTemplate.clear();
	m_textureAssetId = {};
	m_pendingTextureAssetId = parsedTextureAssetId;
	m_isProxyDirty = true;

	return true;
}

bool UIImage::ResolveReferences(SceneBase& scene)
{
	Canvas* resolvedCanvas = m_pCanvas;

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
		if (!assetEntry || assetEntry->type != AssetType::Texture) return false;
	}

	// Set the resolved texture asset
	if (m_pendingTextureAssetId.has_value())
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