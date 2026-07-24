#include <cmath>
#include "SpriteRenderer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Core/Serialization/JsonMath.h"

void SpriteRenderer::OnStartOverride()
{
	// Register this component to the renderer system
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
	if (owner) 
	{
		auto scene = owner->GetOwner();
		if (scene) 
		{
			auto renderSystem = scene->GetRenderSystem();
			if (renderSystem) 
			{
				renderSystem->Unregister(this);
			}
		}
	}
}

bool SpriteRenderer::SetTextureAsset(const Guid& assetId)
{
	// If the asset ID is invalid, clear the texture and mark the proxy as dirty
	if (!assetId.IsValid())
	{
		m_textureAssetId = {};
		m_pendingTextureAssetId.reset();
		m_template = {};
		m_isProxyDirty = true;

		return true;
	}

	// Get the engine context
	EngineContext* context = GetEngineContext();

	if (!context || !context->pAssetManager || !context->pTextureManager)
	{
		return false;
	}

	// Get the asset entry from the asset manager
	const AssetEntry* assetEntry = context->pAssetManager->GetAssetEntry(assetId);

	if (!assetEntry || assetEntry->type != AssetType::Texture)
	{
		return false;
	}

	// Get the texture handle from the asset manager
	const TextureHandle textureHandle = context->pAssetManager->GetTextureHandle(assetId);

	if (textureHandle == InvalidTextureHandle)
	{
		return false;
	}

	// Build the render template for this sprite renderer
	SpriteRenderTemplate renderTemplate;
	renderTemplate.materialDesc.textureHandle = textureHandle;
	renderTemplate.materialDesc.psoKey = PSO_KEY_DEFAULT::SPRITE_TRANSPARENT;
	renderTemplate.materialDesc.baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	renderTemplate.materialDesc.lightingEnabled = false;
	renderTemplate.billboardType = m_billboardType;

	m_template = renderTemplate;
	m_textureAssetId = assetId;
	m_pendingTextureAssetId.reset();
	m_isProxyDirty = true;

	return true;
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
	if (owner) 
	{
		auto transform = owner->GetComponentByClass<Transform>();
		if (transform) 
		{
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

bool SpriteRenderer::Serialize(nlohmann::json& outJson) const
{
	if (!RendererComponent::Serialize(outJson)) return false;

	// Serialize the sprite renderer specific properties
	outJson["uvScale"] = JsonMath::ToJson(m_uvScale);
	outJson["uvOffset"] = JsonMath::ToJson(m_uvOffset);
	outJson["pivot"] = JsonMath::ToJson(m_pivot);
	outJson["billboardType"] = static_cast<int>(m_billboardType);
	outJson["flipX"] = m_flipX;
	outJson["flipY"] = m_flipY;

	// Serialize the texture asset ID, if valid
	if (m_textureAssetId.IsValid())
	{
		outJson["textureAssetId"] = m_textureAssetId.ToString();
	}
	else if (m_pendingTextureAssetId.has_value())
	{
		outJson["textureAssetId"] = m_pendingTextureAssetId->ToString();
	}
	else
	{
		outJson["textureAssetId"] = nullptr;
	}

	return true;
}

bool SpriteRenderer::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object()) return false;

	// Check for required fields in the JSON object
	if (!json.contains("textureAssetId")	||
		!json.contains("uvScale")			||
		!json.contains("uvOffset")			||
		!json.contains("pivot")				||
		!json.contains("billboardType")		||
		!json.contains("flipX")				||
		!json.contains("flipY"))
	{
		return false;
	}

	// Validate the types of the fields
	if (!json["billboardType"].is_number_integer()	||
		!json["flipX"].is_boolean()					||
		!json["flipY"].is_boolean())
	{
		return false;
	}

	Vector2 parsedUVScale;
	Vector2 parsedUVOffset;
	Vector2 parsedPivot;

	int parsedBillboardType = 0;
	bool parsedFlipX = false;
	bool parsedFlipY = false;

	std::optional<Guid> parsedTextureAssetId;

	// Try to parse the billboard type, flipX, and flipY values from the JSON object
	try
	{
		parsedBillboardType = json["billboardType"].get<int>();
		parsedFlipX = json["flipX"].get<bool>();
		parsedFlipY = json["flipY"].get<bool>();
	}
	catch (const nlohmann::json::exception&)
	{
		return false;
	}

	// Validate and parse the texture asset ID
	if (!JsonMath::TryRead(json["uvScale"], parsedUVScale)		||
		!JsonMath::TryRead(json["uvOffset"], parsedUVOffset)	||
		!JsonMath::TryRead(json["pivot"], parsedPivot))
	{
		return false;
	}

	// Validate that the parsed billboard type is within the valid range of the BillboardType enum
	constexpr int kBillboardTypeMin = static_cast<int>(BillboardType::None);
	constexpr int kBillboardTypeMax = static_cast<int>(BillboardType::Cylindrical);

	if (parsedBillboardType < kBillboardTypeMin ||
		parsedBillboardType > kBillboardTypeMax)
	{
		return false;
	}

	// Validate that the UV scale, UV offset, and pivot values are finite numbers
	const auto isFiniteVector2 =
		[](const Vector2& value)
		{
			return std::isfinite(value.x) &&
				std::isfinite(value.y);
		};

	if (!isFiniteVector2(parsedUVScale)		||
		!isFiniteVector2(parsedUVOffset)	||
		!isFiniteVector2(parsedPivot))
	{
		return false;
	}

	// Validate that the pivot values are within the range [0.0, 1.0]
	if (parsedPivot.x < 0.0f ||
		parsedPivot.x > 1.0f ||
		parsedPivot.y < 0.0f ||
		parsedPivot.y > 1.0f)
	{
		return false;
	}

	// Validate and parse the texture asset ID from the JSON object
	const auto& assetJson = json["textureAssetId"];

	if (assetJson.is_null())
	{
		parsedTextureAssetId.reset();
	}
	else
	{
		if (!assetJson.is_string()) return false;

		Guid parsedGuid;

		try
		{
			if (!Guid::TryParse(
				assetJson.get<std::string>(),
				parsedGuid) ||
				!parsedGuid.IsValid())
			{
				return false;
			}
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}

		parsedTextureAssetId = parsedGuid;
	}

	// Deserialize the base RendererComponent properties
	if (!RendererComponent::Deserialize(json)) return false;

	// Assign the parsed values to the member variables of the SpriteRenderer
	m_uvScale = parsedUVScale;
	m_uvOffset = parsedUVOffset;
	m_pivot = parsedPivot;
	m_billboardType = static_cast<BillboardType>(parsedBillboardType);
	m_flipX = parsedFlipX;
	m_flipY = parsedFlipY;

	m_template = {};
	m_template.billboardType = m_billboardType;

	m_textureAssetId = {};
	m_pendingTextureAssetId = parsedTextureAssetId;

	m_isProxyDirty = true;

	return true;
}

bool SpriteRenderer::ResolveReferences(SceneBase& scene)
{
	// If there is no pending texture asset ID, there is nothing to resolve
	if (!m_pendingTextureAssetId.has_value()) return true;

	// Get owner actor and check if it belongs to the provided scene
	Actor* owner = GetOwner();
	if (!owner || owner->GetOwner()!= &scene) return false;

	// Get the engine context from the scene and check if the asset manager and texture manager are available
	EngineContext* context = scene.GetEngineContext();
	if (!context || !context->pAssetManager || !context->pTextureManager) return false;

	// Get the asset entry for the pending texture asset ID and check if it is a valid texture asset
	const Guid& assetId = *m_pendingTextureAssetId;
	const AssetEntry* assetEntry = context->pAssetManager->GetAssetEntry(assetId);
	if (!assetEntry || assetEntry->type != AssetType::Texture) return false;

	// Attempt to set the texture asset using the resolved asset ID
	return SetTextureAsset(assetId);
}