#include "MeshRenderer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Resource/AssetManager.h" 
#include "Engine/Resource/MeshManager.h" 
#include "Engine/Core/Context/Context.h"
#include "Engine/UI/canvas.h"
#include "Engine/Component/RectTransform.h"
#include <algorithm>
#include <cmath>
#include <utility>

bool MeshRenderer::SetMeshAsset(const Guid& assetId)
{
	PreparedMeshAssetState prepared;
	if (PrepareMeshAssetState(assetId, prepared) != AssetPrepareResult::Ready)
	{
		return false;
	}

	CommitMeshAssetState(std::move(prepared));
	return true;
}

MeshRenderer::AssetPrepareResult MeshRenderer::PrepareMeshAssetState(
	const Guid& assetId,
	PreparedMeshAssetState& outState) const
{
	if (!assetId.IsValid())
	{
		outState = {};
		return AssetPrepareResult::Ready;
	}

	const EngineContext* context = GetEngineContext();
	if (!context || !context->pAssetManager || !context->pMeshManager)
	{
		return AssetPrepareResult::Failed;
	}

	const AssetEntry* assetEntry = context->pAssetManager->GetAssetEntry(assetId);
	if (!assetEntry)
	{
		return AssetPrepareResult::MissingAsset;
	}
	if (assetEntry->type != AssetType::Mesh)
	{
		return AssetPrepareResult::Failed;
	}

	const MeshHandle meshHandle = context->pAssetManager->GetMeshHandle(assetId);
	if (meshHandle == InvalidMeshHandle)
	{
		return AssetPrepareResult::Failed;
	}

	const MeshGPU* meshGPU = context->pMeshManager->GetMeshGPU(meshHandle);
	if (!meshGPU)
	{
		return AssetPrepareResult::Failed;
	}

	const MeshMaterialInfo materialInfo = context->pMeshManager->GetMeshMaterialInfo(meshHandle);
	SubmeshRenderTemplate renderTemplate;
	renderTemplate.meshDesc.meshHandle = meshHandle;
	renderTemplate.meshDesc.boundsCenter = meshGPU->GetBoundsCenter();
	renderTemplate.meshDesc.boundsRadius = meshGPU->GetBoundsRadius();
	renderTemplate.materialDesc.textureHandle = materialInfo.textureHandle;
	renderTemplate.materialDesc.psoKey = PSO_KEY_DEFAULT::MESH_OPAQUE;
	renderTemplate.materialDesc.baseColor = materialInfo.materialColor;

	PreparedMeshAssetState prepared;
	prepared.assetId = assetId;
	prepared.templates = { std::move(renderTemplate) };
	outState = std::move(prepared);
	return AssetPrepareResult::Ready;
}

void MeshRenderer::CommitMeshAssetState(PreparedMeshAssetState&& state)
{
	m_meshAssetId = state.assetId;
	m_templates.swap(state.templates);
	m_pendingMeshAssetId.reset();
	m_isProxyDirty = true;
}

Guid MeshRenderer::GetAssetId() const
{
	if (m_meshAssetId.IsValid()) return m_meshAssetId;

	return m_pendingMeshAssetId.value_or(Guid{});
}

void MeshRenderer::OnAttachOverride()
{
	auto* owner = GetOwner();
	if (!owner) return;

	auto* scene = owner->GetOwner();
	if (!scene) return;

	auto* renderSystem = scene->GetRenderSystem();
	if (!renderSystem) return;

	renderSystem->Register(this);
}

void MeshRenderer::OnStartOverride()
{
}

void MeshRenderer::PreUpdateOverride(float deltaTime)
{
}

void MeshRenderer::UpdateOverride(float deltaTime)
{
}

void MeshRenderer::LateUpdateOverride(float deltaTime)
{
}

void MeshRenderer::OnDetachOverride()
{
	auto* owner = GetOwner();
	if (!owner) return;

	auto* scene = owner->GetOwner();
	if (!scene) return;

	auto* renderSystem = scene->GetRenderSystem();
	if (!renderSystem) return;

	renderSystem->Unregister(this);
}

void MeshRenderer::OnDestroyOverride()
{
}

const MeshRendererProxy& MeshRenderer::GetRenderProxy()
{
	auto owner = GetOwner();
	auto transform = owner ? owner->GetComponentByClass<Transform>() : nullptr;

	if (transform && m_transformGeneration != transform->GetWorldGeneration())
	{
		m_isProxyDirty = true;
	}

	if (m_isProxyDirty)
	{
		RebuildRenderProxy();
		if (transform) m_transformGeneration = transform->GetWorldGeneration();
		m_isProxyDirty = false;
	}
	return m_proxy;
}

void MeshRenderer::RebuildRenderProxy()
{
	auto owner = GetOwner();
	if (owner) 
	{
		auto transform = owner->GetComponentByClass<Transform>();
		if (transform) 
		{
			m_proxy.common.position = transform->GetWorldPosition();
			m_proxy.common.worldMatrix = BuildWorldMatrix(transform);
			m_proxy.common.renderSpace = GetRenderSpace();

			Canvas* governingCanvas = GetGoverningCanvas();
			m_proxy.common.color = m_color;
			m_proxy.common.visible = m_isVisible;
		}
	}
}


bool MeshRenderer::ResolveReferences(SceneBase& scene)
{
	if (!m_pendingMeshAssetId.has_value()) return true;

	// Check if the owner actor is valid and belongs to the given scene
	Actor* owner = GetOwner();
	if (!owner || owner->GetOwner() != &scene) return false;

	const Guid assetId = *m_pendingMeshAssetId;
	PreparedMeshAssetState prepared;
	const AssetPrepareResult result = PrepareMeshAssetState(assetId, prepared);
	if (result == AssetPrepareResult::MissingAsset)
	{
		return true;
	}
	if (result != AssetPrepareResult::Ready)
	{
		return false;
	}

	CommitMeshAssetState(std::move(prepared));
	return true;
}

Matrix4x4 MeshRenderer::BuildWorldMatrix(Transform* transform) const
{
	if (!transform) return Matrix4x4::Identity();

	RectTransform* rectTransform = dynamic_cast<RectTransform*>(transform);

	if (rectTransform && GetGoverningCanvas())
	{
		float modelRadius = 0.0f;

		for (const auto& renderTemplate : m_templates)
		{
			const MeshDesc& meshDesc = renderTemplate.meshDesc;

			// Build a sphere around the Actor origin that contains
			// the submesh's own bounding sphere.
			const float radiusFromOrigin = meshDesc.boundsCenter.Length() + meshDesc.boundsRadius;

			modelRadius = (std::max)(modelRadius, radiusFromOrigin);
		}

		Transform3D renderTransform = rectTransform->GetWorldTransform();

		const Vector2 rectSize = rectTransform->GetSize();

		const float rectShortSide = (std::min)(rectSize.x, rectSize.y);

		const float modelDiameter = modelRadius * 2.0f;

		if (modelDiameter > 0.0001f && std::isfinite(modelDiameter))
		{
			const float fitScale = rectShortSide / modelDiameter;

			renderTransform.scale *= fitScale;
		}

		return renderTransform.GetMatrix();
	}
	else
	{
		return transform->GetWorldMatrix();
	}
}

AssetReference<MeshAsset> MeshRenderer::GetMeshAssetReference() const
{
	AssetReference<MeshAsset> value;
	value.SetValue({ GetAssetId(), AssetType::Mesh, m_meshAssetId.IsValid() });
	return value;
}

bool MeshRenderer::SetPendingMeshAssetReference(const AssetReference<MeshAsset>& value)
{
	m_templates.clear();
	m_meshAssetId = {};
	m_pendingMeshAssetId.reset();
	if (value.HasValue()) m_pendingMeshAssetId = value.GetGuid();
	m_isProxyDirty = true;
	return true;
}

bool MeshRenderer::TrySetMeshAssetReference(const AssetReference<MeshAsset>& value)
{
	Actor* owner = GetOwner();
	// Deserialization produces unresolved references. Keep those pending even if
	// an existing Component is currently attached to a Scene.
	if (!owner || !owner->GetOwner() || (value.HasValue() && !value.IsResolved()))
	{
		return SetPendingMeshAssetReference(value);
	}

	PreparedMeshAssetState prepared;
	const Guid assetId = value.HasValue() ? value.GetGuid() : Guid{};
	if (PrepareMeshAssetState(assetId, prepared) != AssetPrepareResult::Ready)
	{
		return false;
	}

	CommitMeshAssetState(std::move(prepared));
	return true;
}
