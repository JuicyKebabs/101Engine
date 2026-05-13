#include "Engine/UI/UIRenderer.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Component/Transform.h"

void UIRenderer::OnStartOverride()
{
	InitialRegistration();
}

void UIRenderer::PreUpdateOverride(float deltaTime)
{
}

void UIRenderer::UpdateOverride(float deltaTime)
{
}

void UIRenderer::LateUpdateOverride(float deltaTime)
{
}

void UIRenderer::OnDestroyOverride()
{
	// Unregister from the render system of the scene
	auto owner = GetOwner();
	if (owner) {
		auto scene = owner->GetOwner();
		if (scene) {
			if (auto rs = scene->GetRenderSystem())
				rs->Unregister(this);
		}
	}

	// Unregister from the canvas
	if(m_pCanvas) m_pCanvas->UnregisterUIRenderer(this);
}

const UIRendererProxy& UIRenderer::GetRenderProxy()
{
	CheckIfTransformChanged();
	RebuildRenderProxy();
	return m_renderProxy;
}

bool UIRenderer::IsVisible() const
{
	return m_isVisible && m_pCanvas && m_pCanvas->IsVisible();
}

void UIRenderer::RebuildRenderProxy()
{
	if (m_isProxyDirty)
	{
		auto owner = GetOwner();
		if (owner) {
			auto transform = owner->GetComponentByClass<Transform>();
			if (transform) {
				m_renderProxy.common.position = transform->GetWorldPosition();
				m_renderProxy.common.worldMatrix = transform->GetWorldMatrix();
			}
		}
		m_renderProxy.common.color = m_color;
		m_renderProxy.common.visible = m_isVisible;
		m_renderProxy.canvasOrder = m_canvasOrder;
		m_renderProxy.order = m_order;
		m_renderProxy.uvScale = m_uvScale;
		m_renderProxy.uvOffset = m_uvOffset;
		m_renderProxy.pivot = m_pivot;
		m_renderProxy.flip.x = m_flipX ? -1.0f : 1.0f;
		m_renderProxy.flip.y = m_flipY ? -1.0f : 1.0f;
		m_isProxyDirty = false;
	}
}

void UIRenderer::InitialRegistration()
{
	if (!m_pCanvas) return;

	// Register with the render system of the scene
	auto owner = GetOwner();
	if (owner)
	{
		auto scene = owner->GetOwner();
		if (scene)
		{
			auto renderSystem = scene->GetRenderSystem();
			if (renderSystem)
			{
				renderSystem->Register(this);
			}
			else
			{
				DBG("UIRenderer component '%s' failed to register with render system. Render system not found in scene.", GetName().c_str());
				return;
			}
		}
		else
		{
			DBG("UIRenderer component '%s' has no scene. Please add it to a scene to function properly.", GetName().c_str());
			return;
		}
	}
	else
	{
		DBG("UIRenderer component '%s' has no owning actor. Please attach it to an actor to function properly.", GetName().c_str());
		return;
	}

	m_pCanvas->RegisterUIRenderer(this); // Register with the canvas for sorting and rendering
}
