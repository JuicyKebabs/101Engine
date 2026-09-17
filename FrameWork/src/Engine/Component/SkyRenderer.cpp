#include "SkyRenderer.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Graphics/RenderSystem.h"
#include "Engine/Graphics/CameraSystem.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/AssetManager.h"
#include "Transform.h"
#include <cmath>
#include <utility>

bool SkyRenderer::SetAsActiveSkyRenderer()
{
	auto owner = GetOwner();

	if (!owner)
	{
		return false;
	}

	auto scene = owner->GetOwner();

	if (!scene)
	{
		return false;
	}

	auto renderSystem = scene->GetRenderSystem();

	if (!renderSystem)
	{
		return false;
	}

	return renderSystem->SetActiveSkyRenderer(this);
}

bool SkyRenderer::SetSkyTextureAsset(const Guid& texture)
{
	PreparedTextureAssetState prepared;

	if (PrepareSkyRendererState(prepared, texture) != AssetPrepareResult::Ready)
	{
		return false;
	}

	CommitSkyTextureState(std::move(prepared));
	return true;
}

void SkyRenderer::SetFollowMode(FollowMode mode)
{
	if (m_followMode == mode)
	{
		return;
	}

	m_followMode = mode;
	InvalidateFollowCache();
}

bool SkyRenderer::SetFollowActor(Actor* actor)
{
	if (!m_followActor.Set(actor))
	{
		return false;
	}

	InvalidateFollowCache();
	return true;
}

bool SkyRenderer::SetFollowActor(const Guid& guid)
{
	if (!m_followActor.SetGuid(guid))
	{
		return false;
	}

	InvalidateFollowCache();
	return true;
}

void SkyRenderer::SetFollowActorReference(const ActorReference& actor)
{
	m_followActor = actor;
	InvalidateFollowCache();
}

void SkyRenderer::OnAttachOverride()
{
	EngineContext* context = GetEngineContext();

	if (!context || !context->pMeshManager)
	{
		return;
	}

	MeshManager* meshManager = context->pMeshManager;
	const MeshHandle sphereMesh = meshManager->LoadDefaultMesh(DefaultMesh::Sphere);
	MeshGPU* meshGPU = meshManager->GetMeshGPU(sphereMesh);

	if (sphereMesh == InvalidMeshHandle || !meshGPU)
	{
		return;
	}

	m_template.meshDesc.meshHandle = sphereMesh;
	m_template.meshDesc.boundsCenter = meshGPU->GetBoundsCenter();
	m_template.meshDesc.boundsRadius = meshGPU->GetBoundsRadius();

	if (m_template.meshDesc.boundsRadius > 0.0f &&
		std::isfinite(m_template.meshDesc.boundsRadius))
	{
		m_sphereScale = 100.0f / m_template.meshDesc.boundsRadius;
	}

	m_template.materialDesc.psoKey = PSO_KEY_DEFAULT::MESH_SKY;
	m_template.materialDesc.psoKey.depth = DepthMode::Disable;
	m_template.materialDesc.psoKey.cull = CullMode::None;
	m_template.materialDesc.psoKey.blend = GetBlendMode();
	m_template.materialDesc.baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	m_template.materialDesc.lightingEnabled = false;
	m_isProxyDirty = true;
}

void SkyRenderer::UpdateOverride(float)
{
	RefreshFollowState();
}

void SkyRenderer::OnDetachOverride()
{
	Actor* owner = GetOwner();
	SceneBase* scene = owner ? owner->GetOwner() : nullptr;
	RenderSystem* renderSystem = scene ? scene->GetRenderSystem() : nullptr;

	if (renderSystem)
	{
		renderSystem->ClearActiveSkyRenderer(this);
	}
}

void SkyRenderer::OnDestroyOverride()
{
	Actor* owner = GetOwner();
	SceneBase* scene = owner ? owner->GetOwner() : nullptr;
	RenderSystem* renderSystem = scene ? scene->GetRenderSystem() : nullptr;

	if (renderSystem)
	{
		renderSystem->ClearActiveSkyRenderer(this);
	}
}

const MeshRendererProxy& SkyRenderer::GetRenderProxy()
{
	if (m_isProxyDirty)
	{
		RebuildRenderProxy();
		m_isProxyDirty = false;
	}

	return m_proxy;
}

bool SkyRenderer::IsConfigured() const
{
	return m_template.meshDesc.meshHandle != InvalidMeshHandle &&
		m_template.materialDesc.textureHandle != InvalidTextureHandle;
}

AssetPrepareResult SkyRenderer::PrepareSkyRendererState(
	PreparedTextureAssetState& outPreparedState,
	const Guid& textureId) const
{
	if (!textureId.IsValid())
	{
		outPreparedState = {};
		return AssetPrepareResult::Ready;
	}

	const EngineContext* context = GetEngineContext();

	if (!context || !context->pAssetManager || !context->pTextureManager)
	{
		return AssetPrepareResult::Failed;
	}

	const AssetEntry* assetEntry = context->pAssetManager->GetAssetEntry(textureId);

	if (!assetEntry)
	{
		return AssetPrepareResult::MissingAsset;
	}

	if (assetEntry->type != AssetType::Texture)
	{
		return AssetPrepareResult::Failed;
	}

	const TextureHandle textureHandle = context->pAssetManager->GetTextureHandle(textureId);

	if (textureHandle == InvalidTextureHandle)
	{
		return AssetPrepareResult::Failed;
	}

	outPreparedState.assetId = textureId;
	outPreparedState.textureHandle = textureHandle;
	return AssetPrepareResult::Ready;
}

void SkyRenderer::CommitSkyTextureState(PreparedTextureAssetState&& state)
{
	m_skyTextureId = state.assetId;
	m_template.materialDesc.textureHandle = state.textureHandle;
	m_pendingSkyTextureId.reset();
	m_isProxyDirty = true;
}

Guid SkyRenderer::GetSkyTextureAssetId() const
{
	if (m_skyTextureId.IsValid())
	{
		return m_skyTextureId;
	}

	return m_pendingSkyTextureId.value_or(Guid{});
}

AssetReference<TextureAsset> SkyRenderer::GetSkyTextureAssetReference() const
{
	AssetReference<TextureAsset> value;
	value.SetValue({ GetSkyTextureAssetId(), AssetType::Texture, m_skyTextureId.IsValid() });
	return value;
}

bool SkyRenderer::SetPendingSkyTextureAssetReference(const AssetReference<TextureAsset>& value)
{
	m_skyTextureId = {};
	m_pendingSkyTextureId.reset();
	m_template.materialDesc.textureHandle = InvalidTextureHandle;

	if (value.HasValue())
	{
		m_pendingSkyTextureId = value.GetGuid();
	}

	m_isProxyDirty = true;
	return true;
}

bool SkyRenderer::TrySetSkyTextureAssetReference(const AssetReference<TextureAsset>& value)
{
	Actor* owner = GetOwner();

	if (!owner || !owner->GetOwner() || (value.HasValue() && !value.IsResolved()))
	{
		return SetPendingSkyTextureAssetReference(value);
	}

	PreparedTextureAssetState prepared;
	const Guid assetId = value.HasValue() ? value.GetGuid() : Guid{};

	if (PrepareSkyRendererState(prepared, assetId) != AssetPrepareResult::Ready)
	{
		return false;
	}

	CommitSkyTextureState(std::move(prepared));
	return true;
}

bool SkyRenderer::ResolveReferences(SceneBase& scene)
{
	Actor* owner = GetOwner();

	if (!owner || owner->GetOwner() != &scene)
	{
		return false;
	}

	if (m_pendingSkyTextureId.has_value())
	{
		PreparedTextureAssetState prepared;
		const AssetPrepareResult result = PrepareSkyRendererState(prepared, *m_pendingSkyTextureId);

		if (result != AssetPrepareResult::MissingAsset)
		{
			if (result != AssetPrepareResult::Ready)
			{
				return false;
			}

			CommitSkyTextureState(std::move(prepared));
		}
	}

	if (m_followActor.HasValue() && !m_followActor.Resolve(scene))
	{
		return false;
	}

	InvalidateFollowCache();
	return true;
}

void SkyRenderer::InvalidateFollowCache()
{
	m_cachedFollowActorHandle = ActorHandle::Null();
	m_cachedFollowTransformGeneration = static_cast<uint64_t>(-1);
	m_isProxyDirty = true;
}

Transform* SkyRenderer::ResolveFollowTransform(Actor*& outActor)
{
	outActor = nullptr;
	Actor* owner = GetOwner();
	SceneBase* scene = owner ? owner->GetOwner() : nullptr;

	switch (m_followMode)
	{
	case FollowMode::Owner:
		outActor = owner;
		break;
	case FollowMode::MainCamera:
		if (scene && scene->GetCameraSystem())
		{
			const Camera* camera = scene->GetCameraSystem()->GetMainCamera();
			outActor = camera ? camera->GetOwner() : nullptr;
		}

		break;
	case FollowMode::Actor:
		if (scene)
		{
			outActor = m_followActor.Resolve(*scene);
		}

		break;
	default:
		break;
	}

	Transform* transform = outActor && !outActor->IsDestroyed() ? outActor->GetComponentByClass<Transform>() : nullptr;

	if (m_followMode != FollowMode::Owner && !transform)
	{
		m_followMode = FollowMode::Owner;
		InvalidateFollowCache();
		outActor = owner;
		transform = owner ? owner->GetComponentByClass<Transform>() : nullptr;
	}

	return transform;
}

void SkyRenderer::RefreshFollowState()
{
	Actor* actor = nullptr;
	Transform* transform = ResolveFollowTransform(actor);
	const ActorHandle handle = actor ? actor->GetHandle() : ActorHandle::Null();
	const uint64_t generation = transform ? transform->GetWorldGeneration() : static_cast<uint64_t>(-1);

	if (handle != m_cachedFollowActorHandle ||
		generation != m_cachedFollowTransformGeneration)
	{
		m_cachedFollowActorHandle = handle;
		m_cachedFollowTransformGeneration = generation;
		m_isProxyDirty = true;
	}
}

void SkyRenderer::RebuildRenderProxy()
{
	auto owner = GetOwner();

	if (!owner)
	{
		DBG("SkyRenderer::RebuildRenderProxy: Owner actor is null.");
		return;
	}

	Actor* followActor = nullptr;
	Transform* followTransform = ResolveFollowTransform(followActor);
	const Vector3 position = followTransform
		? followTransform->GetWorldPosition() : Vector3::Zero();
	const Quaternion rotation = followTransform
		? followTransform->GetWorldRotationQuat() : Quaternion::Identity();
	m_proxy.common.position = position;
	m_proxy.common.worldMatrix = Matrix4x4::CreateTRS(position, rotation, Vector3(m_sphereScale));
	m_proxy.common.renderSpace = RenderSpace::World;

	m_proxy.common.color = m_color;
	m_proxy.common.visible = m_isVisible;
}

void SkyRenderer::ApplyBlendModeToRenderTemplates()
{
	m_template.materialDesc.psoKey.blend = GetBlendMode();
}
