#include <cmath>
#include <utility>
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

AssetPrepareResult SpriteRenderer::PrepareTextureAssetState(
	const Guid& assetId,
	PreparedTextureAssetState& outState) const
{
	if (!assetId.IsValid())
	{
		outState = {};
		outState.renderTemplate.billboardType = m_billboardType;
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

	SpriteRenderTemplate renderTemplate;
	renderTemplate.materialDesc.textureHandle = textureHandle;
	renderTemplate.materialDesc.psoKey = PSO_KEY_DEFAULT::SPRITE_TRANSPARENT;
	renderTemplate.materialDesc.psoKey.blend = GetBlendMode();
	renderTemplate.materialDesc.baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	renderTemplate.materialDesc.lightingEnabled = false;
	renderTemplate.billboardType = m_billboardType;

	PreparedTextureAssetState prepared;
	prepared.assetId = assetId;
	prepared.renderTemplate = std::move(renderTemplate);
	outState = std::move(prepared);
	return AssetPrepareResult::Ready;
}

void SpriteRenderer::CommitTextureAssetState(PreparedTextureAssetState&& state)
{
	m_textureAssetId = state.assetId;
	std::swap(m_template, state.renderTemplate);
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

	if (m_billboardType != BillboardType::None)
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

			switch (m_billboardType)
			{
			case BillboardType::None:
				m_proxy.common.worldMatrix = transform->GetWorldMatrix();
				break;
			case BillboardType::Spherical:
				m_proxy.common.worldMatrix =
					transform->GetWorldMatrix().ToBillboard(cameraInfo.position, cameraInfo.up);
				break;
			case BillboardType::Cylindrical:
				m_proxy.common.worldMatrix =
					transform->GetWorldMatrix().ToCylindricalBillboard(cameraInfo.position, cameraInfo.up);
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
	value.SetValue({ GetTextureAssetId(), AssetType::Texture, m_textureAssetId.IsValid() });
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

void SpriteRenderer::ApplyBlendModeToRenderTemplates()
{
	m_template.materialDesc.psoKey.blend = GetBlendMode();
}
