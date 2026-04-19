#include "MeshRenderer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"

void MeshRenderer::OnStart()
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

void MeshRenderer::PreUpdate(float deltaTime)
{
}

void MeshRenderer::Update(float deltaTime)
{
}

void MeshRenderer::LateUpdate(float deltaTime)
{
}

void MeshRenderer::OnDestroy()
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

void MeshRenderer::Flush()
{
	CheckIfTransformChanged();
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

void MeshRenderer::Initialize(std::vector<SubmeshRenderTemplate> templates)
{
	if (m_isConfigured) return;	// Prevent re-initialization if already configured

	m_templates = std::move(templates);
	m_isConfigured = true;
}

void MeshRenderer::RebuildRenderProxy()
{
	auto owner = GetOwner();
	if (owner) {
		auto transform = owner->GetTransform();
		if (transform) {
			m_proxy.position = transform->GetWorldPosition();
			m_proxy.worldMatrix = transform->GetWorldMatrix();
			m_proxy.color = m_color;
			m_proxy.visible = m_isVisible;
		}
	}
}

void MeshRenderer::CheckIfTransformChanged()
{
	auto owner = GetOwner();
	if (owner) {
		auto transform = owner->GetTransform();
		if (transform) {
			uint64_t currentGeneration = transform->GetWorldGeneration();
			if(m_transformGeneration != currentGeneration) {
				m_transformGeneration = currentGeneration;
				m_isProxyDirty = true;
			}
		}
	}
}