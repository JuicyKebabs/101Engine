#include "PersistentComponentMetadata.h"
#include "PersistentEnumMetadata.h"
#include "PersistentMetadataHelpers.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/UI/Canvas.h"
#include "Engine/UI/UIImage.h"
#include <cmath>

template<class T>
void PersistentComponentMetadata::AddRendererProperties(TypeMetadataBuilder<T>& builder, bool writeLegacySortOrder)
{
	PersistentMetadata::AddComponentName(builder);
	builder.Property("color", &T::GetColor, &T::SetColor).Inspector(InspectorMetadata{.presentation = InspectorPresentation::Color});
	builder.Property("visible", &T::IsVisible, &T::SetVisible);
	// UI order is authoritative; retain the legacy JSON field without applying it twice.
	builder.Property("sortOrderInCanvas", &T::GetSortOrderInCanvas,
		[writeLegacySortOrder](T& c, std::uint32_t v) { if (writeLegacySortOrder) c.SetSortOrderInCanvas(v); }).Optional();
}

template<class T>
void PersistentComponentMetadata::AddUIRendererProperties(TypeMetadataBuilder<T>& builder)
{
	AddRendererProperties(builder, false);
	builder.Property("order", &T::GetOrder, &T::SetOrder);
	builder.Property("uvScale", &T::GetUVScale, &T::SetUVScale).Validate(ValueValidation::Finite2);
	builder.Property("uvOffset", &T::GetUVOffset, &T::SetUVOffset).Validate(ValueValidation::Finite2);
	builder.Property("flipX", &T::IsFlipX, &T::SetFlipX);
	builder.Property("flipY", &T::IsFlipY, &T::SetFlipY);
	builder.template Accessor<ActorReference>("canvasActorId",
		[](const T& c, ActorReference& v) { return c.GetCanvasActorReference(v); },
		[](T& c, const ActorReference& v) { c.SetCanvasActorReference(v); return true; });
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::MeshRenderer(std::string stableTypeName)
{
	TypeMetadataBuilder<::MeshRenderer> builder(std::move(stableTypeName));
	AddRendererProperties(builder);
	builder.Property("meshAssetId", &::MeshRenderer::GetMeshAssetReference, &::MeshRenderer::SetMeshAssetReference);
	return PersistentMetadata::Finish(builder);
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::SpriteRenderer(std::string stableTypeName)
{
	using T = ::SpriteRenderer;
	TypeMetadataBuilder<T> builder(std::move(stableTypeName));
	AddRendererProperties(builder);
	builder.Property("uvScale", &T::GetUVScale, &T::SetUVScale).Validate(ValueValidation::Finite2);
	builder.Property("uvOffset", &T::GetUVOffset, &T::SetUVOffset).Validate(ValueValidation::Finite2);
	builder.Property("pivot", &T::GetPivot, &T::SetPivot).Validate(ValueValidation::UnitCoordinate2);
	builder.Property("billboardType", &T::GetBillboardType, &T::SetBillboardType).SerializedAs(EnumSerializationFormat::Integer);
	builder.Property("flipX", &T::IsFlipX, &T::SetFlipX);
	builder.Property("flipY", &T::IsFlipY, &T::SetFlipY);
	builder.Property("textureAssetId", &T::GetTextureAssetReference, &T::SetTextureAssetReference);
	return PersistentMetadata::Finish(builder);
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::UIRenderer(std::string stableTypeName)
{
	TypeMetadataBuilder<::UIRenderer> builder(std::move(stableTypeName));
	AddUIRendererProperties(builder);
	return PersistentMetadata::Finish(builder);
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::UIImage(std::string stableTypeName)
{
	TypeMetadataBuilder<::UIImage> builder(std::move(stableTypeName));
	AddUIRendererProperties(builder);
	builder.Property("textureAssetId", &::UIImage::GetTextureAssetReference, &::UIImage::SetTextureAssetReference);
	return PersistentMetadata::Finish(builder);
}

std::unique_ptr<TypeMetadata> PersistentComponentMetadata::Canvas(std::string stableTypeName)
{
	using T = ::Canvas;
	TypeMetadataBuilder<T> builder(std::move(stableTypeName));
	PersistentMetadata::AddComponentName(builder);
	builder.Property("renderMode", &T::GetAuthoredRenderMode, &T::SetAuthoredRenderMode).SerializedAs(EnumSerializationFormat::Integer);
	builder.Property("scaleMode", &T::GetScaleMode, &T::SetScaleMode).SerializedAs(EnumSerializationFormat::Integer).Optional();
	builder.Property("sortOrder", &T::GetSortOrder, &T::SetSortOrder);
	builder.Property("visible", &T::IsVisible, &T::SetVisible);
	builder.Property("referenceSize", &T::GetReferenceSize, &T::SetReferenceSize)
		.Validate([](const Vector2& v) { return ValueValidation::Finite2(v) && v.x > 0 && v.y > 0; });
	builder.Property("matchWidthOrHeight", &T::GetMatchWidthOrHeight, &T::SetMatchWidthOrHeight)
		.Optional().Validate([](float v) { return std::isfinite(v) && v >= 0 && v <= 1; });
	return PersistentMetadata::Finish(builder);
}
