#include "MeshRenderer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/RenderAssetResources.h"
#include "Engine/Graphics/RenderingConversions.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Component/RectTransform.h"
#include <algorithm>
#include <cmath>

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

AssetPrepareResult MeshRenderer::PrepareMeshAssetState(const Guid& assetId, PreparedMeshAssetState& output) const
{
    PreparedMeshAssetState prepared;
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
    if (entry && entry->type != AssetType::Mesh)
    {
        return AssetPrepareResult::Failed;
    }

    // Missing files/individual failed SubMeshes are cached Invalid values.
    // Holding/copying failure still propagates so Scene preparation can cancel.
    SceneAssetAcquisition acquisition(*context->pAssetManager, *scene);
    const auto& geometries = context->pAssetManager->GetGeometryInfos(*scene, assetId);
    prepared.assetId = assetId;
    prepared.templates.reserve(geometries.size());
    for (const GeometryInfo& geometry : geometries)
    {
        SubmeshRenderTemplate renderTemplate;
        renderTemplate.meshDesc.meshHandle = geometry.geometry;
        renderTemplate.localToModel = geometry.localToModel;
        renderTemplate.materialDesc.textureHandle = geometry.texture;
        renderTemplate.materialDesc.screen = GetRenderSpace() == RenderSpace::Screen;
        if (context->pRenderAssets)
        {
            Vector2 uvScale = Vector2::One();
            Vector2 uvOffset;
            Vector2 pivot;
            pivot.x = 0.5f;
            pivot.y = 0.5f;
            renderTemplate.materialDesc.description = context->pRenderAssets->MakeMaterial(true,
                renderTemplate.materialDesc.screen, geometry.texture, uvScale, uvOffset, pivot, false, false);
            renderTemplate.materialDesc.handle = context->pAssetManager->RegisterSceneMaterial(*scene,
                renderTemplate.materialDesc.description);
        }

        if (context->pRenderer)
        {
            rendering::GeometryData snapshot;
            const auto queried = context->pRenderer->GetGeometryData(geometry.geometry, snapshot);
            if (queried.IsSuccess())
            {
                renderTemplate.meshDesc.boundsCenter = EngineRendering::FromRenderer(snapshot.boundsCenter);
                renderTemplate.meshDesc.boundsRadius = snapshot.boundsRadius;
                renderTemplate.meshDesc.boundsRevision = snapshot.revision;
            }
        }

        prepared.templates.push_back(std::move(renderTemplate));
    }

    acquisition.Commit();
    output = std::move(prepared);
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
	if (m_meshAssetId.IsValid())
	{
		return m_meshAssetId;
	}

	return m_pendingMeshAssetId.value_or(Guid{});
}

void MeshRenderer::OnAttachOverride()
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

		if (transform)
		{
			m_transformGeneration = transform->GetWorldGeneration();
		}

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

// Resolve the existing generic SceneLoader route before publication. Asset
// availability failure is a display fallback; wrong ownership/type remains failure.
bool MeshRenderer::ResolveReferences(SceneBase& scene)
{
    if (!m_pendingMeshAssetId)
    {
        return true;
    }

    Actor* actor = GetOwner();
    if (!actor || actor->GetOwner() != &scene)
    {
        return false;
    }

    PreparedMeshAssetState prepared;
    if (PrepareMeshAssetState(*m_pendingMeshAssetId, prepared) != AssetPrepareResult::Ready)
    {
        return false;
    }

    CommitMeshAssetState(std::move(prepared));
    return true;
}

Matrix4x4 MeshRenderer::BuildWorldMatrix(Transform* transform) const
{
	if (!transform)
	{
		return Matrix4x4::Identity();
	}

	RectTransform* rectTransform = dynamic_cast<RectTransform*>(transform);

	if (rectTransform && GetGoverningCanvas())
	{
		float modelRadius = 0.0f;

		for (const auto& renderTemplate : m_templates)
		{
			const MeshDesc& meshDesc = renderTemplate.meshDesc;

            // Include each model node placement without changing the legacy
            // unit-transform fit scale. The norm bound also covers node shear.
            const Matrix4x4& placement = renderTemplate.localToModel;
            const Vector3 centerInModel = placement.TransformPoint(meshDesc.boundsCenter);
            const float radiusFromOrigin = centerInModel.Length()
                + meshDesc.boundsRadius * EngineRendering::ConservativeLinearScale(placement);

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
	AssetReferenceValue reference;
    reference.guid = GetAssetId();
    reference.expectedType = AssetType::Mesh;
    reference.resolved = m_meshAssetId.IsValid();
    value.SetValue(reference);
	return value;
}

bool MeshRenderer::SetPendingMeshAssetReference(const AssetReference<MeshAsset>& value)
{
	m_templates.clear();
	m_meshAssetId = {};
	m_pendingMeshAssetId.reset();

	if (value.HasValue())
	{
		m_pendingMeshAssetId = value.GetGuid();
	}

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

// Called inside the existing extraction walk immediately before Submit.
// This is not a second whole-Scene scan or a GPU readiness polling operation.
bool MeshRenderer::SynchronizeRenderResources()
{
    const EngineContext* context = GetEngineContext();
    if (!context || !context->pRenderer)
    {
        return true;
    }

    const bool screen = GetRenderSpace() == RenderSpace::Screen;
    for (SubmeshRenderTemplate& renderTemplate : m_templates)
    {
        rendering::GeometryData snapshot;
        const auto queried = context->pRenderer->GetGeometryData(renderTemplate.meshDesc.meshHandle, snapshot);
        if (queried.IsSuccess() && snapshot.revision != renderTemplate.meshDesc.boundsRevision)
        {
            renderTemplate.meshDesc.boundsCenter = EngineRendering::FromRenderer(snapshot.boundsCenter);
            renderTemplate.meshDesc.boundsRadius = snapshot.boundsRadius;
            renderTemplate.meshDesc.boundsRevision = snapshot.revision;
            m_isProxyDirty = true;
        }
        else if (!queried.IsSuccess())
        {
            renderTemplate.meshDesc.boundsRevision = 0;
        }

        if (context->pRenderAssets && renderTemplate.materialDesc.screen != screen)
        {
            auto description = renderTemplate.materialDesc.description;
            description.pipeline = context->pRenderAssets->MeshPipeline(screen);
            const auto updated = context->pRenderer->UpdateMaterial(renderTemplate.materialDesc.handle, description);
            if (!updated.IsSuccess())
            {
                return false;
            }

            renderTemplate.materialDesc.description = std::move(description);
            renderTemplate.materialDesc.screen = screen;
        }
    }

    return true;
}
