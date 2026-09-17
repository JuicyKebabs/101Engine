#include "Engine/UI/UIImage.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/UI/Canvas.h"
#include <utility>

bool UIImage::SetTextureAsset(const Guid& assetId)
{
	PreparedTextureAssetState prepared;

	if (PrepareTextureAssetState(assetId, prepared) != AssetPrepareResult::Ready)
	{
		return false;
	}

	CommitTextureAssetState(std::move(prepared));
	return true;
}

UIImage::AssetPrepareResult UIImage::PrepareTextureAssetState(
	const Guid& assetId,
	PreparedTextureAssetState& outState) const
{
	if (!assetId.IsValid())
	{
		outState = {};
		return AssetPrepareResult::Ready;
	}

	const EngineContext* context = GetEngineContext();

	if (!context || !context->pAssetManager || !context->pTextureManager)
	{
		return AssetPrepareResult::Failed;
	}

	const AssetEntry* assetEntry = context->pAssetManager->GetAssetEntry(assetId);

	if (!assetEntry)
	{
		return AssetPrepareResult::MissingAsset;
	}

	if (assetEntry->type != AssetType::Texture)
	{
		return AssetPrepareResult::Failed;
	}

	const TextureHandle textureHandle = context->pAssetManager->GetTextureHandle(assetId);

	if (textureHandle == InvalidTextureHandle)
	{
		return AssetPrepareResult::Failed;
	}

	UIRenderElement element;
	element.materialDesc.textureHandle = textureHandle;
	element.materialDesc.psoKey = PSO_KEY_DEFAULT::UI;
	element.materialDesc.psoKey.blend = GetBlendMode();
	element.materialDesc.baseColor = { 1, 1, 1, 1 };
	element.materialDesc.lightingEnabled = false;

	PreparedTextureAssetState prepared;
	prepared.assetId = assetId;
	prepared.renderTemplate = { std::move(element) };
	outState = std::move(prepared);
	return AssetPrepareResult::Ready;
}

void UIImage::CommitTextureAssetState(PreparedTextureAssetState&& state)
{
	m_textureAssetId = state.assetId;
	m_renderTemplate.swap(state.renderTemplate);
	m_pendingTextureAssetId.reset();
	m_isProxyDirty = true;
}

bool UIImage::ResolveReferences(SceneBase& scene)
{
	Canvas* resolvedCanvas = GetGoverningCanvas();
	PreparedTextureAssetState preparedTexture;
	AssetPrepareResult textureResult = AssetPrepareResult::Ready;

	// Resolve the canvas actor if a pending canvas actor ID is set
	if (m_pendingCanvasActorId.has_value())
	{
		Actor* canvasActor = scene.ResolveActor(*m_pendingCanvasActorId);

		if (!canvasActor)
		{
			return false;
		}

		resolvedCanvas = canvasActor->GetComponentByClass<Canvas>();

		if (!resolvedCanvas)
		{
			return false;
		}
	}

	// Resolve the texture asset if a pending texture asset ID is set
	if (m_pendingTextureAssetId.has_value())
	{
		Actor* owner = GetOwner();

		if (!owner || owner->GetOwner() != &scene)
		{
			return false;
		}

		textureResult = PrepareTextureAssetState(*m_pendingTextureAssetId, preparedTexture);

		if (textureResult == AssetPrepareResult::Failed)
		{
			return false;
		}
	}

	if (m_pendingTextureAssetId.has_value() && textureResult == AssetPrepareResult::Ready)
	{
		CommitTextureAssetState(std::move(preparedTexture));
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

AssetReference<TextureAsset> UIImage::GetTextureAssetReference() const
{
	AssetReference<TextureAsset> value;
	value.SetValue({ GetTextureAssetId(), AssetType::Texture, m_textureAssetId.IsValid() });
	return value;
}

bool UIImage::SetPendingTextureAssetReference(const AssetReference<TextureAsset>& value)
{
	m_renderTemplate.clear();
	m_textureAssetId = {};
	m_pendingTextureAssetId.reset();

	if (value.HasValue())
	{
		m_pendingTextureAssetId = value.GetGuid();
	}

	m_isProxyDirty = true;
	return true;
}

bool UIImage::TrySetTextureAssetReference(const AssetReference<TextureAsset>& value)
{
	Actor* owner = GetOwner();
	// Preserve deferred deserialization while allowing validated Inspector
	// selections to prepare their runtime state immediately.
	if (!owner || !owner->GetOwner() || (value.HasValue() && !value.IsResolved()))
	{
		return SetPendingTextureAssetReference(value);
	}

	PreparedTextureAssetState prepared;
	const Guid assetId = value.HasValue() ? value.GetGuid() : Guid{};

	if (PrepareTextureAssetState(assetId, prepared) != AssetPrepareResult::Ready)
	{
		return false;
	}

	CommitTextureAssetState(std::move(prepared));
	return true;
}
