#pragma once
#include "nlohmann/json_fwd.hpp"
#include <typeindex>

//-----------------------------------------------------------------------------------------------------------
// Reflection serialization and deserialization system
// This system provides a way to serialize and deserialize objects based on their reflection metadata.
// Serializer convert given type-erased value to JSON object with suitable expression
// Deserializer convert JSON object to type-erased value without knowing the concrete type
// (concrete type is hidden within the accessor callbacks in the PropertyMetadata).
//-----------------------------------------------------------------------------------------------------------

class TypeMetadata;
class ActorReferenceCodec;
class ActorReferenceSaveContext;
class ActorReferenceRestoreContext;
class AssetReferenceCodec;
class AssetReferenceSaveContext;
class AssetReferenceRestoreContext;

// Policy for handling unknown properties during deserialization.
enum class UnknownPropertyPolicy
{
	Reject,	// Reject unknown properties and stop deserialization with an error.(e.g., for scene loading).
	Ignore,	// Ignore unknown properties and continue deserialization (e.g., for hotreload).
};

// Context for saving and restoreing reflection data,
// including codecs and contexts for ActorReference and AssetReference serialization and deserialization.
struct ReflectionSaveContext
{
	const ActorReferenceCodec* actorReferenceCodec = nullptr;
	const ActorReferenceSaveContext* actorReferenceContext = nullptr;
	const AssetReferenceCodec* assetReferenceCodec = nullptr;
	const AssetReferenceSaveContext* assetReferenceContext = nullptr;
};

struct ReflectionRestoreContext
{
	const ActorReferenceCodec* actorReferenceCodec = nullptr;
	const ActorReferenceRestoreContext* actorReferenceContext = nullptr;
	const AssetReferenceCodec* assetReferenceCodec = nullptr;
	const AssetReferenceRestoreContext* assetReferenceContext = nullptr;
	UnknownPropertyPolicy unknownPropertyPolicy = UnknownPropertyPolicy::Reject;
};

class ReflectionSerializer
{
public:
	static bool Serialize(
		const TypeMetadata& metadata,
		std::type_index objectType,
		const void* object,
		nlohmann::json& outJson,
		ReflectionSaveContext context = {});
};

class ReflectionDeserializer
{
public:
	static bool Deserialize(
		const TypeMetadata& metadata,
		std::type_index objectType,
		const nlohmann::json& json,
		void* object,
		ReflectionRestoreContext context = {});
};
