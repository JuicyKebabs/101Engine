#pragma once

#include <memory>
#include <string>

class TypeMetadata;
template<class ObjectType>
class TypeMetadataBuilder;

class PersistentComponentMetadata
{
public:
	static std::unique_ptr<TypeMetadata> Transform(std::string stableTypeName);
	static std::unique_ptr<TypeMetadata> RectTransform(std::string stableTypeName);
	static std::unique_ptr<TypeMetadata> Camera(std::string stableTypeName);
	static std::unique_ptr<TypeMetadata> Collider(std::string stableTypeName);
	static std::unique_ptr<TypeMetadata> MeshRenderer(std::string stableTypeName);
	static std::unique_ptr<TypeMetadata> SpriteRenderer(std::string stableTypeName);
	static std::unique_ptr<TypeMetadata> UIRenderer(std::string stableTypeName);
	static std::unique_ptr<TypeMetadata> UIImage(std::string stableTypeName);
	static std::unique_ptr<TypeMetadata> Canvas(std::string stableTypeName);

private:

	template<class ComponentType>
	static void AddRendererProperties(
		TypeMetadataBuilder<ComponentType>& builder,
		bool writeLegacySortOrder = true);

	template<class ComponentType>
	static void AddUIRendererProperties(TypeMetadataBuilder<ComponentType>& builder);

};
