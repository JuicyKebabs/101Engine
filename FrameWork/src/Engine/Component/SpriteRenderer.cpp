#include <cmath>
#include <utility>
#include "SpriteRenderer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/RenderAssetResources.h"
#include "Engine/Core/Serialization/JsonMath.h"

void SpriteRenderer::OnAttachOverride()
{
	auto* owner = GetOwner();

	if (!owner)
	{
		return;
	}

	auto* scene = owner->GetOwner();

	if (!scene)
	{
		return;
	}

	auto* renderSystem = scene->GetRenderSystem();

	if (!renderSystem)
	{
		return;
	}

	renderSystem->Register(this);
}

void SpriteRenderer::OnStartOverride()
{
}

void SpriteRenderer::PreUpdateOverride(float deltaTime)
{
}

void SpriteRenderer::UpdateOverride(float deltaTime)
{
}

void SpriteRenderer::LateUpdateOverride(float deltaTime)
{
}

void SpriteRenderer::OnDetachOverride()
{
	auto* owner = GetOwner();

	if (!owner)
	{
		return;
	}

	auto* scene = owner->GetOwner();

	if (!scene)
	{
		return;
	}

	auto* renderSystem = scene->GetRenderSystem();

	if (!renderSystem)
	{
		return;
	}

	renderSystem->Unregister(this);
}

void SpriteRenderer::OnDestroyOverride()
{
}

bool SpriteRenderer::SetTextureAsset(const Guid& assetId)
{
	PreparedTextureAssetState prepared;

	if (PrepareTextureAssetState(assetId, prepared) != AssetPrepareResult::Ready)
	{
		return false;
	}

	CommitTextureAssetState(std::move(prepared));
	return true;
}

// Resource registration belongs to preparation/explicit edits, not OnAttach.
// Each Sprite's Material owns its UV and local-Quad values independently.
AssetPrepareResult SpriteRenderer::PrepareTextureAssetState(
    const Guid& assetId, PreparedTextureAssetState& output) const
{
    PreparedTextureAssetState prepared;
    prepared.assetId = assetId;
    prepared.renderTemplate.billboardType = m_billboardType;
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
    auto& material = prepared.renderTemplate.materialDesc;
    material.textureHandle = context->pAssetManager->GetTextureHandle(*scene, assetId);
    material.screen = GetRenderSpace() == RenderSpace::Screen;
    if (context->pRenderAssets)
    {
        material.description = context->pRenderAssets->MakeMaterial(false, material.screen,
            material.textureHandle, m_uvScale, m_uvOffset, m_pivot, m_flipX, m_flipY);
        material.handle = context->pAssetManager->RegisterSceneMaterial(*scene, material.description);
    }

    acquisition.Commit();
    output = std::move(prepared);
    return AssetPrepareResult::Ready;
}

void SpriteRenderer::CommitTextureAssetState(PreparedTextureAssetState&& state)
{
	m_textureAssetId = state.assetId;
	std::swap(m_template, state.renderTemplate);
    m_materialDirty = false;
	m_pendingTextureAssetId.reset();
	m_isProxyDirty = true;
}

const SpriteRendererProxy& SpriteRenderer::GetRenderProxy(const CameraInfo& cameraInfo)
{
	auto owner = GetOwner();
	auto transform = owner ? owner->GetComponentByClass<Transform>() : nullptr;

	if (transform && m_transformGeneration != transform->GetWorldGeneration())
	{
		m_isProxyDirty = true;
	}


	if (m_isProxyDirty)
	{
		RebuildRenderProxy(cameraInfo);

		if (transform)
		{
			m_transformGeneration = transform->GetWorldGeneration();
		}

		m_isProxyDirty = false;
	}

	return m_proxy;
}

void SpriteRenderer::RebuildRenderProxy(const CameraInfo& cameraInfo)
{
	auto owner = GetOwner();

	if (owner)
	{
		auto transform = owner->GetComponentByClass<Transform>();

		if (transform) 
		{
			m_proxy.common.position = transform->GetWorldPosition();
			m_proxy.common.color = m_color;
			m_proxy.common.visible = m_isVisible;
			m_proxy.common.renderSpace = GetRenderSpace();

			Canvas* governingCanvas = GetGoverningCanvas();
			m_proxy.uvScale = m_uvScale;
			m_proxy.uvOffset = m_uvOffset;
			m_proxy.pivot = m_pivot;
			m_proxy.flip.x = m_flipX ? -1.0f : 1.0f;
			m_proxy.flip.y = m_flipY ? -1.0f : 1.0f;

            // Submit the original Actor World. Renderer derives facing for
            // each Scene Camera; local pivot/flip remains in this Material.
            m_proxy.common.worldMatrix = transform->GetWorldMatrix();

		}
	}
}

bool SpriteRenderer::ResolveReferences(SceneBase& scene)
{
	// If there is no pending texture asset ID, there is nothing to resolve
	if (!m_pendingTextureAssetId.has_value())
	{
		return true;
	}

	// Get owner actor and check if it belongs to the provided scene
	Actor* owner = GetOwner();

	if (!owner || owner->GetOwner() != &scene)
	{
		return false;
	}

	const Guid assetId = *m_pendingTextureAssetId;
	PreparedTextureAssetState prepared;
	const AssetPrepareResult result = PrepareTextureAssetState(assetId, prepared);

	if (result == AssetPrepareResult::MissingAsset)
	{
		return true;
	}

	if (result != AssetPrepareResult::Ready)
	{
		return false;
	}

	CommitTextureAssetState(std::move(prepared));
	return true;
}

AssetReference<TextureAsset> SpriteRenderer::GetTextureAssetReference() const
{
	AssetReference<TextureAsset> value;
	AssetReferenceValue reference;
    reference.guid = GetTextureAssetId();
    reference.expectedType = AssetType::Texture;
    reference.resolved = m_textureAssetId.IsValid();
    value.SetValue(reference);
	return value;
}

bool SpriteRenderer::SetPendingTextureAssetReference(const AssetReference<TextureAsset>& value)
{
	m_template = {};
	m_template.billboardType = m_billboardType;
	m_textureAssetId = {};
	m_pendingTextureAssetId.reset();

	if (value.HasValue())
	{
		m_pendingTextureAssetId = value.GetGuid();
	}

	m_isProxyDirty = true;
	return true;
}

bool SpriteRenderer::TrySetTextureAssetReference(const AssetReference<TextureAsset>& value)
{
	Actor* owner = GetOwner();
	// Deserialization produces unresolved references; the AssetPicker marks its
	// catalog-validated live selections as resolved.
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

bool SpriteRenderer::SynchronizeRenderResources()
{
    const EngineContext* context = GetEngineContext();
    if (!context || !context->pRenderer || !context->pRenderAssets)
    {
        return true;
    }

    auto& material = m_template.materialDesc;
    const bool screen = GetRenderSpace() == RenderSpace::Screen;
    if (!m_materialDirty && material.screen == screen)
    {
        return true;
    }

    if (material.handle == rendering::MaterialHandle{})
    {
        m_materialDirty = false;
        return true; // Keep the diagnosed Renderer Material fallback; do not re-register every Frame.
    }

    auto description = context->pRenderAssets->MakeMaterial(false, screen, material.textureHandle,
        m_uvScale, m_uvOffset, m_pivot, m_flipX, m_flipY);
    const auto updated = context->pRenderer->UpdateMaterial(material.handle, description);
    if (!updated.IsSuccess())
    {
        DBG("Sprite Material update: %s", updated.message.c_str());
        return false;
    }

    material.description = std::move(description);
    material.screen = screen;
    m_materialDirty = false;
    return true;
}
