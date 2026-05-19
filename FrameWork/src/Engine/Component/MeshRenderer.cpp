#include "MeshRenderer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"

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