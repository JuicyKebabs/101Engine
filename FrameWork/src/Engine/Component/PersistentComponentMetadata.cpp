#include "PersistentComponentMetadata.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Resource/AssetReference.h"
#include "Engine/UI/Canvas.h"
#include "Engine/UI/UIImage.h"
#include "Engine/UI/UIRenderer.h"
#include <cmath>

namespace
{
	constexpr PropertyPolicy kPolicy = PropertyPolicy::Serializable | PropertyPolicy::Inspectable;

	template<class ComponentType>
	std::unique_ptr<TypeMetadata> Finish(TypeMetadataBuilder<ComponentType>& builder)
	{
		auto metadata = builder.Build();
		if (!metadata) return nullptr;
		return std::make_unique<TypeMetadata>(std::move(*metadata));
	}

	bool IsFinite(const Vector2& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y);
	}

	bool IsNormalizedCoordinate(const Vector2& value)
	{
		if (!IsFinite(value))
		{
			return false;
		}
		const bool xInRange = value.x >= 0.0f && value.x <= 1.0f;
		const bool yInRange = value.y >= 0.0f && value.y <= 1.0f;
		return xInRange && yInRange;
	}

	bool HasPositiveSize(const Vector2& value)
	{
		return IsFinite(value) && value.x > 0.0f && value.y > 0.0f;
	}

}

template<class ComponentType>
void PersistentComponentMetadata::AddName(TypeMetadataBuilder<ComponentType>& builder)
{
	builder.template AddAccessorProperty<std::string>(
		"name", PropertyLogicalType::String, kPolicy,
		[](const ComponentType& component, std::string& value)
		{
			value = component.GetName();
			return true;
		},
		[](ComponentType& component, const std::string& value)
		{
			component.SetName(value);
			return true;
		});
}

template<class ComponentType>
void PersistentComponentMetadata::AddRendererProperties(
	TypeMetadataBuilder<ComponentType>& builder,
	bool writeLegacySortOrder)
{
		AddName(builder);
		builder.template AddAccessorProperty<Vector4>(
			"color", PropertyLogicalType::Color, kPolicy,
			[](const ComponentType& component, Vector4& value)
			{
				value = component.GetColor();
				return true;
			},
			[](ComponentType& component, const Vector4& value)
			{
				component.SetColor(value);
				return true;
			});
		builder.template AddAccessorProperty<bool>(
			"visible", PropertyLogicalType::Bool, kPolicy,
			[](const ComponentType& component, bool& value)
			{
				value = component.IsVisible();
				return true;
			},
			[](ComponentType& component, bool value)
			{
				component.SetVisible(value);
				return true;
			});
		builder.template AddAccessorProperty<std::uint32_t>(
			"sortOrderInCanvas", PropertyLogicalType::UnsignedInteger, kPolicy,
			[](const ComponentType& component, std::uint32_t& value)
			{
				value = component.GetSortOrderInCanvas();
				return true;
			},
			[writeLegacySortOrder](ComponentType& component, std::uint32_t value)
			{
				if (writeLegacySortOrder) component.SetSortOrderInCanvas(value);
				return true;
			},
			{}, {}, PropertyRequirement::Optional);
}

template<class ComponentType>
void PersistentComponentMetadata::AddUIRendererProperties(TypeMetadataBuilder<ComponentType>& builder)
{
		AddRendererProperties(builder, false);
		builder.template AddAccessorProperty<std::uint32_t>(
			"order", PropertyLogicalType::UnsignedInteger, kPolicy,
			[](const ComponentType& component, std::uint32_t& value)
			{
				value = component.GetOrder();
				return true;
			},
			[](ComponentType& component, std::uint32_t value)
			{
				component.SetOrder(value);
				return true;
			});
		builder.template AddAccessorProperty<Vector2>(
			"uvScale", PropertyLogicalType::Vector2, kPolicy,
			[](const ComponentType& component, Vector2& value) { value = component.GetUVScale(); return true; },
			[](ComponentType& component, const Vector2& value)
			{
				if (!IsFinite(value)) return false;
				component.SetUVScale(value);
				return true;
			});
		builder.template AddAccessorProperty<Vector2>(
			"uvOffset", PropertyLogicalType::Vector2, kPolicy,
			[](const ComponentType& component, Vector2& value) { value = component.GetUVOffset(); return true; },
			[](ComponentType& component, const Vector2& value)
			{
				if (!IsFinite(value)) return false;
				component.SetUVOffset(value);
				return true;
			});
		builder.template AddAccessorProperty<bool>(
			"flipX", PropertyLogicalType::Bool, kPolicy,
			[](const ComponentType& component, bool& value) { value = component.IsFlipX(); return true; },
			[](ComponentType& component, bool value) { component.SetFlipX(value); return true; });
		builder.template AddAccessorProperty<bool>(
			"flipY", PropertyLogicalType::Bool, kPolicy,
			[](const ComponentType& component, bool& value) { value = component.IsFlipY(); return true; },
			[](ComponentType& component, bool value) { component.SetFlipY(value); return true; });
		builder.template AddAccessorProperty<ActorReference>(
			"canvasActorId", PropertyLogicalType::ActorReference, kPolicy,
			[](const ComponentType& component, ActorReference& value)
			{
				if (::Canvas* canvas = component.GetCanvas())
				{
					Actor* actor = canvas->GetOwner();
					return actor && value.Set(actor);
				}
				if (component.m_pendingCanvasActorId)
				{
					return value.SetGuid(*component.m_pendingCanvasActorId);
				}
				value.Clear();
				return true;
			},
			[](ComponentType& component, const ActorReference& value)
			{
				component.SetCanvas(nullptr);
				component.m_pendingCanvasActorId.reset();
				if (value.HasValue()) component.m_pendingCanvasActorId = value.GetGuid();
				return true;
			});
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::MeshRenderer()
{
	TypeMetadataBuilder<::MeshRenderer> builder("MeshRenderer");
	AddRendererProperties(builder);
	builder.AddAccessorProperty<AssetReference<MeshAsset>>(
		"meshAssetId", PropertyLogicalType::AssetReference, kPolicy,
		[](const ::MeshRenderer& component, AssetReference<MeshAsset>& value)
		{
			const Guid guid = component.GetAssetId();
			if (!guid.IsValid()) return true;
			return value.SetGuid(guid);
		},
		[](::MeshRenderer& component, const AssetReference<MeshAsset>& value)
		{
			component.m_templates.clear();
			component.m_meshAssetId = {};
			component.m_pendingMeshAssetId.reset();
			if (value.HasValue()) component.m_pendingMeshAssetId = value.GetGuid();
			component.m_isProxyDirty = true;
			return true;
		});
	return Finish(builder);
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::SpriteRenderer()
{
	TypeMetadataBuilder<::SpriteRenderer> builder("SpriteRenderer");
	AddRendererProperties(builder);
	auto billboard = EnumMetadataBuilder<BillboardType>()
		.Add("None", BillboardType::None).Add("Spherical", BillboardType::Spherical)
		.Add("Cylindrical", BillboardType::Cylindrical).Build();
	if (!billboard) return nullptr;
	builder.AddAccessorProperty<Vector2>(
		"uvScale", PropertyLogicalType::Vector2, kPolicy,
		[](const ::SpriteRenderer& component, Vector2& value) { value = component.GetUVScale(); return true; },
		[](::SpriteRenderer& component, const Vector2& value) { if (!IsFinite(value)) return false; component.SetUVScale(value); return true; });
	builder.AddAccessorProperty<Vector2>(
		"uvOffset", PropertyLogicalType::Vector2, kPolicy,
		[](const ::SpriteRenderer& component, Vector2& value) { value = component.GetUVOffset(); return true; },
		[](::SpriteRenderer& component, const Vector2& value) { if (!IsFinite(value)) return false; component.SetUVOffset(value); return true; });
	builder.AddAccessorProperty<Vector2>(
		"pivot", PropertyLogicalType::Vector2, kPolicy,
		[](const ::SpriteRenderer& component, Vector2& value) { value = component.GetPivot(); return true; },
		[](::SpriteRenderer& component, const Vector2& value)
		{
			if (!IsNormalizedCoordinate(value))
			{
				return false;
			}
			component.SetPivot(value); return true;
		});
	builder.AddAccessorProperty<BillboardType>(
		"billboardType", PropertyLogicalType::Enum, kPolicy,
		[](const ::SpriteRenderer& component, BillboardType& value) { value = component.GetBillboardType(); return true; },
		[](::SpriteRenderer& component, BillboardType value) { component.SetBillboardType(value); return true; },
		*billboard, EnumSerializationFormat::Integer);
	builder.AddAccessorProperty<bool>(
		"flipX", PropertyLogicalType::Bool, kPolicy,
		[](const ::SpriteRenderer& component, bool& value) { value = component.IsFlipX(); return true; },
		[](::SpriteRenderer& component, bool value) { component.SetFlipX(value); return true; });
	builder.AddAccessorProperty<bool>(
		"flipY", PropertyLogicalType::Bool, kPolicy,
		[](const ::SpriteRenderer& component, bool& value) { value = component.IsFlipY(); return true; },
		[](::SpriteRenderer& component, bool value) { component.SetFlipY(value); return true; });
	builder.AddAccessorProperty<AssetReference<TextureAsset>>(
		"textureAssetId", PropertyLogicalType::AssetReference, kPolicy,
		[](const ::SpriteRenderer& component, AssetReference<TextureAsset>& value)
		{
			const Guid guid = component.GetTextureAssetId();
			if (!guid.IsValid()) return true;
			return value.SetGuid(guid);
		},
		[](::SpriteRenderer& component, const AssetReference<TextureAsset>& value)
		{
			component.m_template = {};
			component.m_template.billboardType = component.m_billboardType;
			component.m_textureAssetId = {};
			component.m_pendingTextureAssetId.reset();
			if (value.HasValue()) component.m_pendingTextureAssetId = value.GetGuid();
			component.m_isProxyDirty = true;
			return true;
		});
	return Finish(builder);
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::UIRenderer()
{
	TypeMetadataBuilder<::UIRenderer> builder("UIRenderer");
	AddUIRendererProperties(builder);
	return Finish(builder);
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::UIImage()
{
	TypeMetadataBuilder<::UIImage> builder("UIImage");
	AddUIRendererProperties(builder);
	builder.AddAccessorProperty<AssetReference<TextureAsset>>(
		"textureAssetId", PropertyLogicalType::AssetReference, kPolicy,
		[](const ::UIImage& component, AssetReference<TextureAsset>& value)
		{
			const Guid guid = component.GetTextureAssetId();
			if (!guid.IsValid()) return true;
			return value.SetGuid(guid);
		},
		[](::UIImage& component, const AssetReference<TextureAsset>& value)
		{
			component.m_renderTemplate.clear();
			component.m_textureAssetId = {};
			component.m_pendingTextureAssetId.reset();
			if (value.HasValue()) component.m_pendingTextureAssetId = value.GetGuid();
			component.m_isProxyDirty = true;
			return true;
		});
	return Finish(builder);
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::Canvas()
{
	TypeMetadataBuilder<::Canvas> builder("Canvas");
	AddName(builder);
	auto renderMode = EnumMetadataBuilder<CanvasRenderMode>()
		.Add("ScreenSpace", CanvasRenderMode::ScreenSpace)
		.Add("WorldSpace", CanvasRenderMode::WorldSpace).Build();
	auto scaleMode = EnumMetadataBuilder<CanvasScaleMode>()
		.Add("ConstantPixelSize", CanvasScaleMode::ConstantPixelSize)
		.Add("ScaleWithScreenSize", CanvasScaleMode::ScaleWithScreenSize).Build();
	if (!renderMode || !scaleMode) return nullptr;
	builder.AddAccessorProperty<CanvasRenderMode>(
		"renderMode", PropertyLogicalType::Enum, kPolicy,
		[](const ::Canvas& component, CanvasRenderMode& value) { value = component.m_authoredRenderMode; return true; },
		[](::Canvas& component, CanvasRenderMode value) { component.SetAuthoredRenderMode(value); return true; },
		*renderMode, EnumSerializationFormat::Integer);
	builder.AddAccessorProperty<CanvasScaleMode>(
		"scaleMode", PropertyLogicalType::Enum, kPolicy,
		[](const ::Canvas& component, CanvasScaleMode& value) { value = component.m_scaleMode; return true; },
		[](::Canvas& component, CanvasScaleMode value) { component.SetScaleMode(value); return true; },
		*scaleMode, EnumSerializationFormat::Integer, PropertyRequirement::Optional);
	builder.AddAccessorProperty<std::uint32_t>(
		"sortOrder", PropertyLogicalType::UnsignedInteger, kPolicy,
		[](const ::Canvas& component, std::uint32_t& value) { value = component.m_sortOrder; return true; },
		[](::Canvas& component, std::uint32_t value) { component.SetSortOrder(value); return true; });
	builder.AddAccessorProperty<bool>(
		"visible", PropertyLogicalType::Bool, kPolicy,
		[](const ::Canvas& component, bool& value) { value = component.m_isVisible; return true; },
		[](::Canvas& component, bool value) { component.SetVisible(value); return true; });
	builder.AddAccessorProperty<Vector2>(
		"referenceSize", PropertyLogicalType::Vector2, kPolicy,
		[](const ::Canvas& component, Vector2& value) { value = component.m_referenceSize; return true; },
		[](::Canvas& component, const Vector2& value)
		{
			if (!HasPositiveSize(value))
			{
				return false;
			}
			component.SetReferenceSize(value); return true;
		});
	builder.AddAccessorProperty<float>(
		"matchWidthOrHeight", PropertyLogicalType::Float, kPolicy,
		[](const ::Canvas& component, float& value) { value = component.m_matchWidthOrHeight; return true; },
		[](::Canvas& component, float value)
		{
			if (!std::isfinite(value) || value < 0.0f || value > 1.0f) return false;
			component.SetMatchWidthOrHeight(value); return true;
		}, {}, {}, PropertyRequirement::Optional);
	return Finish(builder);
}
