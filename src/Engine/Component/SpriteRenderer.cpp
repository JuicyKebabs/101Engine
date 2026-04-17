#include "SpriteRenderer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"

void SpriteRenderer::OnStart()
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

void SpriteRenderer::PreUpdate(float deltaTime)
{
}

void SpriteRenderer::Update(float deltaTime)
{
}

void SpriteRenderer::LateUpdate(float deltaTime)
{
}

void SpriteRenderer::OnDestroy()
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

void SpriteRenderer::Flush()
{
	CheckIfTransformChanged();
}

const SpriteRendererProxy& SpriteRenderer::GetRenderProxy()
{
	if (m_isProxyDirty)
	{
		RebuildRenderProxy();
	}
	return m_proxy;
}

void SpriteRenderer::RebuildRenderProxy()
{
	auto owner = GetOwner();
	if (owner) {
		auto transform = owner->GetTransform();
		if (transform) {
			m_proxy.position = transform->GetWorldPosition();
			m_proxy.worldMatrix = transform->GetWorldMatrix();
			m_proxy.color = m_color;
			m_proxy.uvScale = m_uvScale;
			m_proxy.uvOffset = m_uvOffset;
			m_proxy.pivot = m_pivot;
			m_proxy.visible = m_isVisible;
			m_proxy.flip.x = m_flipX ? -1.0f : 1.0f;
			m_proxy.flip.y = m_flipY ? -1.0f : 1.0f;
		}
	}
}

void SpriteRenderer::CheckIfTransformChanged()
{
	auto owner = GetOwner();
	if (owner) {
		auto transform = owner->GetTransform();
		if (transform) {
			uint64_t currentGeneration = transform->GetWorldGeneration();
			if (m_transformGeneration != currentGeneration) {
				m_transformGeneration = currentGeneration;
				m_isProxyDirty = true;
			}
			else {
				m_isProxyDirty = false;
			}
		}
	}
}
