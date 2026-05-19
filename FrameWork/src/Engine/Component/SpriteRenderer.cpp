#include "SpriteRenderer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"

void SpriteRenderer::OnStartOverride()
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

void SpriteRenderer::PreUpdateOverride(float deltaTime)
{
}

void SpriteRenderer::UpdateOverride(float deltaTime)
{
}

void SpriteRenderer::LateUpdateOverride(float deltaTime)
{
}

void SpriteRenderer::OnDestroyOverride()
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

const SpriteRendererProxy& SpriteRenderer::GetRenderProxy(const CameraInfo& cameraInfo)
{
	if (m_isProxyDirty)
	{
		RebuildRenderProxy(cameraInfo);
		m_isProxyDirty = false;
	}
	return m_proxy;
}

void SpriteRenderer::RebuildRenderProxy(const CameraInfo& cameraInfo)
{
	auto owner = GetOwner();
	if (owner) {
		auto transform = owner->GetComponentByClass<Transform>();
		if (transform) {
			m_proxy.common.position = transform->GetWorldPosition();
			m_proxy.common.color = m_color;
			m_proxy.common.visible = m_isVisible;
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