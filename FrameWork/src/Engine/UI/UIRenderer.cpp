#include <cmath>
#include <limits>
#include "Engine/UI/UIRenderer.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Graphics/RenderSystem.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Core/Serialization/JsonMath.h"

void UIRenderer::OnAttachOverride()
{
	InitialRegistration();
}

void UIRenderer::OnStartOverride()
{
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

void UIRenderer::OnDetachOverride()
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
	Canvas* governingCanvas = GetGoverningCanvas();
	if(governingCanvas) governingCanvas->UnregisterUIRenderer(this);
}

void UIRenderer::OnDestroyOverride()
{
}

const UIRendererProxy& UIRenderer::GetRenderProxy()
{
	CheckIfTransformChanged();
	RebuildRenderProxy();
	return m_renderProxy;
}

bool UIRenderer::IsVisible() const
{
	return GetGoverningCanvas() && RendererComponent::IsVisible();
}

void UIRenderer::RebuildRenderProxy()
{
	if (m_isProxyDirty)
	{
		auto owner = GetOwner();
		if (owner) {
			auto transform = owner->GetComponentByClass<RectTransform>();
			if (transform) {
				m_renderProxy.common.position = transform->GetWorldPosition();
				m_renderProxy.common.worldMatrix = transform->GetWorldMatrix();
			}
		}
		m_renderProxy.common.color = m_color;
		m_renderProxy.common.visible = m_isVisible;
		m_renderProxy.common.renderSpace = GetRenderSpace();
		m_renderProxy.uvScale = m_uvScale;
		m_renderProxy.uvOffset = m_uvOffset;
		m_renderProxy.flip.x = m_flipX ? -1.0f : 1.0f;
		m_renderProxy.flip.y = m_flipY ? -1.0f : 1.0f;
		m_isProxyDirty = false;
	}
}

void UIRenderer::InitialRegistration()
{
	Canvas* governingCanvas = GetGoverningCanvas();

	if (!governingCanvas) return;

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

	governingCanvas->RegisterUIRenderer(this); // Register with the canvas for sorting and rendering
}

void UIRenderer::SetGoverningCanvas(Canvas* canvas)
{
	Canvas* previousCanvas = GetGoverningCanvas();

	if (previousCanvas == canvas)
	{
		m_isProxyDirty = true;
		return;
	}

	if (previousCanvas) previousCanvas->UnregisterUIRenderer(this);

	RendererComponent::SetGoverningCanvas(canvas);

	if (!IsStarted()) return;

	if (canvas)
	{
		InitialRegistration();
		return;
	}

	// If the canvas is set to nullptr, unregister from the render system of the scene
	Actor* owner = GetOwner();
	SceneBase* scene = owner
		? owner->GetOwner()
		: nullptr;

	if (scene)
	{
		if (RenderSystem* renderSystem = scene->GetRenderSystem())
		{
			renderSystem->Unregister(this);
		}
	}
}


bool UIRenderer::ResolveReferences(SceneBase& scene)
{
	if (!m_pendingCanvasActorId.has_value()) return true;

	Actor* canvasActor = scene.ResolveActor(*m_pendingCanvasActorId);

	if (!canvasActor) return false;

	Canvas* canvas = canvasActor->GetComponentByClass<Canvas>();

	if (!canvas) return false;

	SetGoverningCanvas(canvas);
	m_pendingCanvasActorId.reset();
	m_isProxyDirty = true;

	return true;
}
