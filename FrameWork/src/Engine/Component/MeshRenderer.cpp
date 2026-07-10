#include "MeshRenderer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Resource/AssetManager.h" 
#include "Engine/Resource/MeshManager.h" 

void MeshRenderer::SetAsset(const Guid& assetId)
{
	m_assetId = assetId;

	// Get mesh handle stored in AssetManager using the assetId
	MeshHandle meshHandle = AssetManager::GetInstance().GetMeshHandle(assetId);
	SubmeshRenderTemplate templateDesc;
	templateDesc.meshDesc.meshHandle = meshHandle;

	// Get material info from MeshManager using the mesh handle
	auto materialInfo = MeshManager::GetInstance()->GetMeshMaterialInfo(meshHandle);
	templateDesc.materialDesc.textureHandle = materialInfo.textureHandle;
	templateDesc.materialDesc.baseColor = materialInfo.materialColor;

	// Set the render template for this renderer
	m_templates = { templateDesc };
	m_isProxyDirty = true;
}

void MeshRenderer::OnStartOverride()
{
	// Register this component to the renderer system
	auto owner = GetOwner();
	if (owner) {
		auto scene = owner->GetOwner();
		if (scene) {
			auto renderSystem = scene->GetRenderSystem();
			if (renderSystem) {
				renderSystem->Register(this);
			}
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
	if (owner) {
		auto scene = owner->GetOwner();
		if (scene) {
			auto renderSystem = scene->GetRenderSystem();
			if (renderSystem) {
				renderSystem->Unregister(this);
			}
		}
	}
}

const MeshRendererProxy& MeshRenderer::GetRenderProxy()
{
	if (m_isProxyDirty)
	{
		RebuildRenderProxy();
		m_isProxyDirty = false;
	}
	return m_proxy;
}

void MeshRenderer::RebuildRenderProxy()
{
	auto owner = GetOwner();
	if (owner) {
		auto transform = owner->GetComponentByClass<Transform>();
		if (transform) {
			m_proxy.common.position = transform->GetWorldPosition();
			m_proxy.common.worldMatrix = transform->GetWorldMatrix();
			m_proxy.common.color = m_color;
			m_proxy.common.visible = m_isVisible;
		}
	}
}