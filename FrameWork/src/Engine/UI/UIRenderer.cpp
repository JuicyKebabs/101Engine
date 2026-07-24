#include <cmath>
#include <limits>
#include "Engine/UI/UIRenderer.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Core/Serialization/JsonMath.h"

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

UINT UIRenderer::GetCanvasOrder() const
{
	return m_pCanvas ? m_pCanvas->GetSortOrder() : 0;
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
		m_renderProxy.canvasOrder = GetCanvasOrder();
		m_renderProxy.order = m_order;
		m_renderProxy.uvScale = m_uvScale;
		m_renderProxy.uvOffset = m_uvOffset;
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

void UIRenderer::SetCanvas(Canvas* canvas)
{
	if (m_pCanvas == canvas) return;

	if (m_pCanvas) m_pCanvas->UnregisterUIRenderer(this);

	m_pCanvas = canvas;
	m_isProxyDirty = true;

	if (m_pCanvas && IsStarted()) InitialRegistration();
}

bool UIRenderer::Serialize(nlohmann::json& outJson) const
{
	if (!RendererComponent::Serialize(outJson)) return false;

	outJson["order"] = m_order;
	outJson["uvScale"] = JsonMath::ToJson(m_uvScale);
	outJson["uvOffset"] = JsonMath::ToJson(m_uvOffset);
	outJson["flipX"] = m_flipX;
	outJson["flipY"] = m_flipY;

	if (m_pCanvas) 
	{// Serialize the canvas actor's Guid for reference
		Actor* canvasActor = m_pCanvas->GetOwner();

		if (!canvasActor || !canvasActor->GetGuid().IsValid())
		{
			DBG("UIRenderer component '%s' has an invalid canvas actor. Please ensure the canvas is properly set up.", GetName().c_str());
			return false;
		}

		outJson["canvasActorId"] = canvasActor->GetGuid().ToString();
	}
	else if (m_pendingCanvasActorId.has_value())
	{// Serialize the pending canvas actor's Guid for deferred registration
		outJson["canvasActorId"] = m_pendingCanvasActorId->ToString();
	}
	else
	{// No canvas assigned
		outJson["canvasActorId"] = nullptr;
	}

	return true;
}

bool UIRenderer::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object()) return false;

	// Validate for required fields
	if (!json.contains("canvasActorId") ||
		!json.contains("order") ||
		!json.contains("uvScale") ||
		!json.contains("uvOffset") ||
		!json.contains("flipX") ||
		!json.contains("flipY"))
	{
		return false;
	}

	// Validate for types of parameters stored in Json file
	if (!json["order"].is_number_integer() ||
		!json["flipX"].is_boolean() ||
		!json["flipY"].is_boolean())
	{
		return false;
	}

	int64_t parsedOrder = 0;
	Vector2 parsedUVScale;
	Vector2 parsedUVOffset;
	bool parsedFlipX = false;
	bool parsedFlipY = false;

	// Parse 
	std::optional<Guid> parsedCanvasActorId;

	try
	{
		parsedOrder = json["order"].get<int64_t>();
		parsedFlipX = json["flipX"].get<bool>();
		parsedFlipY = json["flipY"].get<bool>();
	}
	catch (const nlohmann::json::exception&)
	{
		return false;
	}

	if (parsedOrder < 0 ||
		static_cast<uint64_t>(parsedOrder) >
		static_cast<uint64_t>((std::numeric_limits<UINT>::max)()))
	{
		return false;
	}

	if (!JsonMath::TryRead(json["uvScale"], parsedUVScale) ||
		!JsonMath::TryRead(json["uvOffset"], parsedUVOffset))
	{
		return false;
	}

	const auto isFiniteVector2 =
		[](const Vector2& value)
		{
			return std::isfinite(value.x) && std::isfinite(value.y);
		};

	if (!isFiniteVector2(parsedUVScale) ||
		!isFiniteVector2(parsedUVOffset))
	{
		return false;
	}

	const auto& canvasJson = json["canvasActorId"];

	if (canvasJson.is_null())
	{
		parsedCanvasActorId.reset();
	}
	else
	{
		if (!canvasJson.is_string()) return false;

		Guid parsedGuid;

		try
		{
			if (!Guid::TryParse(
				canvasJson.get<std::string>(),
				parsedGuid) || !parsedGuid.IsValid())
			{
				return false;
			}
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}

		parsedCanvasActorId = parsedGuid;
	}

	if (!RendererComponent::Deserialize(json)) return false;

	SetCanvas(nullptr);

	m_order = static_cast<UINT>(parsedOrder);
	m_uvScale = parsedUVScale;
	m_uvOffset = parsedUVOffset;
	m_flipX = parsedFlipX;
	m_flipY = parsedFlipY;

	m_pendingCanvasActorId = parsedCanvasActorId;

	m_isProxyDirty = true;

	return true;
}

bool UIRenderer::ResolveReferences(SceneBase& scene)
{
	if (!m_pendingCanvasActorId.has_value()) return true;

	Actor* canvasActor = scene.ResolveActor(*m_pendingCanvasActorId);

	if (!canvasActor) return false;

	Canvas* canvas = canvasActor->GetComponentByClass<Canvas>();

	if (!canvas) return false;

	SetCanvas(canvas);
	m_pendingCanvasActorId.reset();
	m_isProxyDirty = true;

	return true;
}
