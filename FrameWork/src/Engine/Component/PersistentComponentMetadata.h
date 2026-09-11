#pragma once

#include <memory>

class TypeMetadata;
template<class ObjectType>
class TypeMetadataBuilder;

class PersistentComponentMetadata
{
public:
	static std::unique_ptr<TypeMetadata> Transform();
	static std::unique_ptr<TypeMetadata> RectTransform();
	static std::unique_ptr<TypeMetadata> Camera();
	static std::unique_ptr<TypeMetadata> Collider();
	static std::unique_ptr<TypeMetadata> MeshRenderer();
	static std::unique_ptr<TypeMetadata> SpriteRenderer();
	static std::unique_ptr<TypeMetadata> UIRenderer();
	static std::unique_ptr<TypeMetadata> UIImage();
	static std::unique_ptr<TypeMetadata> Canvas();

private:
	template<class ComponentType>
	static void AddName(TypeMetadataBuilder<ComponentType>& builder);

	template<class ComponentType>
	static void AddRendererProperties(
		TypeMetadataBuilder<ComponentType>& builder,
		bool writeLegacySortOrder = true);

	template<class ComponentType>
	static void AddUIRendererProperties(TypeMetadataBuilder<ComponentType>& builder);

};
