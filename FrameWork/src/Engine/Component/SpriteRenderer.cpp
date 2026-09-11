#include <cmath>
#include "SpriteRenderer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Core/Serialization/JsonMath.h"

void SpriteRenderer::OnAttachOverride()
{
	auto* owner = GetOwner();
	if (!owner) return;

	auto* scene = owner->GetOwner();
	if (!scene) return;

	auto* renderSystem = scene->GetRenderSystem();
	if (!renderSystem) return;

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
	if (!owner) return;

	auto* scene = owner->GetOwner();
	if (!scene) return;

	auto* renderSystem = scene->GetRenderSystem();
	if (!renderSystem) return;

	renderSystem->Unregister(this);
}

void SpriteRenderer::OnDestroyOverride()
{
}

bool SpriteRenderer::SetTextureAsset(const Guid& assetId)
{
	// If the asset ID is invalid, clear the texture and mark the proxy as dirty
	if (!assetId.IsValid())
	{
		m_textureAssetId = {};
		m_pendingTextureAssetId.reset();
		m_template = {};
		m_isProxyDirty = true;

		return true;
	}

	// Get the engine context
	EngineContext* context = GetEngineContext();

	if (!context || !context->pAssetManager || !context->pTextureManager)
	{
		return false;
	}

	// Get the asset entry from the asset manager
	const AssetEntry* assetEntry = context->pAssetManager->GetAssetEntry(assetId);

	if (!assetEntry || assetEntry->type != AssetType::Texture)
	{
		return false;
	}

	// Get the texture handle from the asset manager
	const TextureHandle textureHandle = context->pAssetManager->GetTextureHandle(assetId);

	if (textureHandle == InvalidTextureHandle)
	{
		return false;
	}

	// Build the render template for this sprite renderer
	SpriteRenderTemplate renderTemplate;
	renderTemplate.materialDesc.textureHandle = textureHandle;
	renderTemplate.materialDesc.psoKey = PSO_KEY_DEFAULT::SPRITE_TRANSPARENT;
	renderTemplate.materialDesc.baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	renderTemplate.materialDesc.lightingEnabled = false;
	renderTemplate.billboardType = m_billboardType;

	m_template = renderTemplate;
	m_textureAssetId = assetId;
	m_pendingTextureAssetId.reset();
	m_isProxyDirty = true;

	return true;
}

const SpriteRendererProxy& SpriteRenderer::GetRenderProxy(const CameraInfo& cameraInfo)
{
	auto owner = GetOwner();
	auto transform = owner ? owner->GetComponentByClass<Transform>() : nullptr;

	if (transform && m_transformGeneration != transform->GetWorldGeneration())
	{
		m_isProxyDirty = true;
	}

	if (m_billboardType != BillboardType::None)
	{
		m_isProxyDirty = true;
	}

	if (m_isProxyDirty)
	{
		RebuildRenderProxy(cameraInfo);
		if (transform) m_transformGeneration = transform->GetWorldGeneration();
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

			switch (m_billboardType)
			{
			case BillboardType::None:
				m_proxy.common.worldMatrix = transform->GetWorldMatrix();
				break;
			case BillboardType::Spherical:
				m_proxy.common.worldMatrix = transform->GetWorldMatrix().ToBillboard(cameraInfo.position, cameraInfo.up);
				break;
			case BillboardType::Cylindrical:
				m_proxy.common.worldMatrix = transform->GetWorldMatrix().ToCylindricalBillboard(cameraInfo.position, cameraInfo.up);
				break;
			default:
				break;
			}

		}
	}
}


bool SpriteRenderer::ResolveReferences(SceneBase& scene)
{
	// If there is no pending texture asset ID, there is nothing to resolve
	if (!m_pendingTextureAssetId.has_value()) return true;

	// Get owner actor and check if it belongs to the provided scene
	Actor* owner = GetOwner();
	if (!owner || owner->GetOwner()!= &scene) return false;

	// Get the engine context from the scene and check if the asset manager and texture manager are available
	EngineContext* context = scene.GetEngineContext();
	if (!context || !context->pAssetManager || !context->pTextureManager) return false;

	// Get the asset entry for the pending texture asset ID and check if it is a valid texture asset
	const Guid& assetId = *m_pendingTextureAssetId;
	const AssetEntry* assetEntry = context->pAssetManager->GetAssetEntry(assetId);
	if (!assetEntry || assetEntry->type != AssetType::Texture)
	{
		m_template = {};
		m_textureAssetId = {};
		m_isProxyDirty = true;
		return true;
	}

	// Attempt to set the texture asset using the resolved asset ID
	return SetTextureAsset(assetId);
}

AssetReference<TextureAsset> SpriteRenderer::GetTextureAssetReference() const
{
	AssetReference<TextureAsset> value;
	value.SetGuid(GetTextureAssetId());
	return value;
}

void SpriteRenderer::SetTextureAssetReference(const AssetReference<TextureAsset>& value)
{
	m_template = {};
	m_template.billboardType = m_billboardType;
	m_textureAssetId = {};
	m_pendingTextureAssetId.reset();
	if (value.HasValue()) m_pendingTextureAssetId = value.GetGuid();
	m_isProxyDirty = true;
}
