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

bool MeshRenderer::SetMeshAsset(const Guid& assetId)
{
	// Check if the provided assetId is valid
	if (!assetId.IsValid())
	{
		m_meshAssetId = {};
		m_pendingMeshAssetId.reset();
		m_templates.clear();
		m_isProxyDirty = true;

		return true;
	}

	// Check if the engine context and necessary managers are available
	EngineContext* context = GetEngineContext();

	if (!context				||
		!context->pAssetManager ||
		!context->pMeshManager)
	{
		return false;
	}

	// Get asset and validate its type
	const AssetEntry* assetEntry = context->pAssetManager->GetAssetEntry(assetId);

	if (!assetEntry || assetEntry->type != AssetType::Mesh)
	{
		return false;
	}

	// Get the mesh handle from the asset manager
	const MeshHandle meshHandle = context->pAssetManager->GetMeshHandle(assetId);

	if (meshHandle == InvalidMeshHandle) return false;

	MeshGPU* meshGPU = context->pMeshManager->GetMeshGPU(meshHandle);
	if (!meshGPU) return false;

	// Get the mesh material info from the mesh manager
	const MeshMaterialInfo materialInfo = context->pMeshManager->GetMeshMaterialInfo(meshHandle);

	// Create a render template for the mesh
	SubmeshRenderTemplate renderTemplate;
	renderTemplate.meshDesc.meshHandle = meshHandle;
	// MeshGPU stores a sphere centered at the Actor origin that contains
	// every vertex. Pass it to the render template for RectTransform fitting.
	renderTemplate.meshDesc.boundsCenter = meshGPU->GetBoundsCenter();
	renderTemplate.meshDesc.boundsRadius = meshGPU->GetBoundsRadius();
	renderTemplate.materialDesc.textureHandle = materialInfo.textureHandle;
	renderTemplate.materialDesc.psoKey = PSO_KEY_DEFAULT::MESH_OPAQUE;
	renderTemplate.materialDesc.baseColor = materialInfo.materialColor;

	// Update the component's state
	m_templates = { renderTemplate };
	m_meshAssetId = assetId;
	m_pendingMeshAssetId.reset();
	m_isProxyDirty = true;

	return true;
}

Guid MeshRenderer::GetAssetId() const
{
	if (m_meshAssetId.IsValid()) return m_meshAssetId;

	return m_pendingMeshAssetId.value_or(Guid{});
}

void MeshRenderer::OnStartOverride()
{
	// Register this component to the renderer system
	auto owner = GetOwner();
	if (owner) 
	{
		auto scene = owner->GetOwner();
		if (scene) 
		{
			auto renderSystem = scene->GetRenderSystem();
			if (renderSystem) renderSystem->Register(this);
		}
	}
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

void MeshRenderer::OnDestroyOverride()
{
	// Unregister this component from the renderer system
	auto owner = GetOwner();
	if (owner) 
	{
		auto scene = owner->GetOwner();
		if (scene) 
		{
			auto renderSystem = scene->GetRenderSystem();
			if (renderSystem) renderSystem->Unregister(this);
		}
	}
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

bool MeshRenderer::Serialize(nlohmann::json& outJson) const
{
	if (!RendererComponent::Serialize(outJson)) return false;

	if (m_meshAssetId.IsValid())
	{
		outJson["meshAssetId"] = m_meshAssetId.ToString();
	}
	else if (m_pendingMeshAssetId.has_value())
	{
		outJson["meshAssetId"] = m_pendingMeshAssetId->ToString();
	}
	else
	{
		outJson["meshAssetId"] = nullptr;
	}

	return true;
}

bool MeshRenderer::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object()) return false;

	if (!json.contains("meshAssetId")) return false;

	std::optional<Guid> parsedAssetId;

	// Deserialize the asset ID from JSON
	const auto& assetIdJson = json["meshAssetId"];

	if (assetIdJson.is_null())
	{// If the asset ID is null, reset the parsed asset ID
		parsedAssetId.reset();
	}
	else
	{
		if (!assetIdJson.is_string()) return false;

		Guid parsedGuid;

		// Attempt to parse the asset ID string into a Guid
		try
		{
			if (!Guid::TryParse(
				assetIdJson.get<std::string>(),
				parsedGuid) || !parsedGuid.IsValid())
			{
				return false;
			}
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}

		parsedAssetId = parsedGuid;
	}

	// Deserialize the base class
	if (!RendererComponent::Deserialize(json)) return false;

	// Update the component's state based on the parsed asset ID
	// Be ready to load the asset later
	m_templates.clear();
	m_meshAssetId = {};
	m_pendingMeshAssetId = parsedAssetId;
	m_isProxyDirty = true;

	return true;
}

bool MeshRenderer::ResolveReferences(SceneBase& scene)
{
	if (!m_pendingMeshAssetId.has_value()) return true;

	// Check if the owner actor is valid and belongs to the given scene
	Actor* owner = GetOwner();
	if (!owner || owner->GetOwner() != &scene) return false;

	// Check if the engine context and necessary managers are available
	EngineContext* context = scene.GetEngineContext();
	if (!context || !context->pAssetManager || !context->pMeshManager)
	{
		return false;
	}

	// Get the asset entry for the pending asset ID and validate its type
	const Guid assetId = *m_pendingMeshAssetId;
	const AssetEntry* assetEntry = context->pAssetManager->GetAssetEntry(assetId);
	if (!assetEntry || assetEntry->type != AssetType::Mesh)
	{
		// Clear the asset ID and templates if the asset is invalid or not a mesh
		m_templates.clear();
		m_meshAssetId = {};
		m_isProxyDirty = true;

		return true;
	}

	// Attempt to set the mesh asset using the resolved asset ID
	return SetMeshAsset(assetId);
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
