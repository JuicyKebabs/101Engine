#include "Engine/UI/UIImage.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/RenderAssetResources.h"
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
    const Guid& assetId, PreparedTextureAssetState& output) const
{
    PreparedTextureAssetState prepared;
    prepared.assetId = assetId;
    if (!assetId.IsValid())
    {
        output = std::move(prepared);
        return AssetPrepareResult::Ready;
    }

    Actor* actor = GetOwner();
    SceneBase* scene = actor ? actor->GetOwner() : nullptr;
    const EngineContext* context = GetEngineContext();
    if (!scene || !context || !context->pAssetManager)
    {
        return AssetPrepareResult::Failed;
    }

    const AssetEntry* entry = context->pAssetManager->GetAssetEntry(assetId);
    if (entry && entry->type != AssetType::Texture)
    {
        return AssetPrepareResult::Failed;
    }

    SceneAssetAcquisition acquisition(*context->pAssetManager, *scene);
    UIRenderElement element;
    element.materialDesc.textureHandle = context->pAssetManager->GetTextureHandle(*scene, assetId);
    element.materialDesc.screen = GetRenderSpace() == RenderSpace::Screen;
    if (context->pRenderAssets)
    {
        Vector2 centerPivot;
        centerPivot.x = 0.5f;
        centerPivot.y = 0.5f;
        element.materialDesc.description = context->pRenderAssets->MakeMaterial(false,
            element.materialDesc.screen, element.materialDesc.textureHandle,
            m_uvScale, m_uvOffset, centerPivot, m_flipX, m_flipY);
        element.materialDesc.handle = context->pAssetManager->RegisterSceneMaterial(*scene, element.materialDesc.description);
    }

    prepared.renderTemplate.push_back(std::move(element));
    acquisition.Commit();
    output = std::move(prepared);
    return AssetPrepareResult::Ready;
}

void UIImage::CommitTextureAssetState(PreparedTextureAssetState&& state)
{
	m_textureAssetId = state.assetId;
	m_renderTemplate.swap(state.renderTemplate);
    m_materialDirty = false;
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
	AssetReferenceValue reference;
    reference.guid = GetTextureAssetId();
    reference.expectedType = AssetType::Texture;
    reference.resolved = m_textureAssetId.IsValid();
    value.SetValue(reference);
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
