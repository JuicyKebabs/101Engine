#include "ComponentReflection.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Component.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/Reflection/ActorReferenceCodec.h"
#include "Engine/Core/Reflection/AssetReferenceCodec.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Core/Reflection/ReflectionSerialization.h"
#include "Engine/Resource/AssetManagerAssetReferenceContext.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneActorReferenceContext.h"
#include "Engine/Scene/SceneBase.h"
#include <string>
#include <utility>

namespace
{
	class DirectActorReferenceContext final : public ActorReferenceSaveContext
	{
	public:
		ActorReferenceCodecResult Validate(const Guid& guid) const override
		{
			if (!guid.IsValid())
			{
				return ActorReferenceCodecResult::ActorNotFound;
			}
			return ActorReferenceCodecResult::Success;
		}
	};

	class DirectAssetReferenceContext final : public AssetReferenceSaveContext
	{
	public:
		AssetReferenceCodecResult Validate(const Guid& guid, AssetType type) const override
		{
			const bool validReference = guid.IsValid() && type != AssetType::Unknown;
			if (!validReference)
			{
				return AssetReferenceCodecResult::AssetNotFound;
			}
			return AssetReferenceCodecResult::Success;
		}
	};

	class MissingAssetReferenceContext final : public AssetReferenceSaveContext
	{
	public:
		AssetReferenceCodecResult Validate(const Guid&, AssetType) const override
		{
			return AssetReferenceCodecResult::AssetNotFound;
		}
	};

	void SetMetadataError(ReflectionError* outError, std::string message)
	{
		if (!outError)
		{
			return;
		}
		*outError = {
			ReflectionErrorCode::InvalidMetadata,
			std::nullopt,
			std::move(message) };
	}
}

bool SerializeReflectedComponent(
	const Component& component,
	nlohmann::json& outJson,
	const SceneBase* scene,
	ReflectionError* outError)
{
	// Get the TypeMetadata for the component's type from the ComponentRegistry.
	const TypeMetadata* metadata = ComponentRegistry::Get().GetMetadata(typeid(component));
	if (!metadata)
	{
		SetMetadataError(outError, "Component reflection metadata is not registered.");
		return false;
	}

	// Prepare the serialization context with codecs and contexts for ActorReference and AssetReference.
	GuidActorReferenceCodec actorCodec;
	AssetReferenceCodec assetCodec;
	DirectActorReferenceContext directActorContext;
	DirectAssetReferenceContext directAssetContext;
	MissingAssetReferenceContext missingAssetContext;
	ReflectionSaveContext context{
		.actorReferenceCodec = &actorCodec,
		.actorReferenceContext = &directActorContext,
		.assetReferenceCodec = &assetCodec,
		.assetReferenceContext = &directAssetContext,
	};

	// Prepare context for scene and asset manager if a scene is provided.
	std::optional<SceneActorReferenceContext> sceneActorContext;
	std::optional<AssetManagerAssetReferenceContext> assetContext;
	if (scene)
	{
		sceneActorContext.emplace(*scene);
		context.actorReferenceContext = &*sceneActorContext;
		context.assetReferenceContext = &missingAssetContext;

		EngineContext* engineContext = scene->GetEngineContext();
		if (engineContext && engineContext->pAssetManager)
		{
			assetContext.emplace(*engineContext->pAssetManager);
			context.assetReferenceContext = &*assetContext;
		}
	}

	return ReflectionSerializer::Serialize(
		*metadata, typeid(component), &component, outJson, context, outError);
}

bool DeserializeReflectedComponent(
	Component& component,
	const nlohmann::json& json,
	ReflectionError* outError)
{
	ComponentRegistry& registry = ComponentRegistry::Get();
	const std::type_index type = typeid(component);
	const TypeMetadata* metadata = registry.GetMetadata(type);
	if (!metadata)
	{
		SetMetadataError(outError, "Component reflection metadata is not registered.");
		return false;
	}

	const std::string typeName = registry.GetNameByTypeIndex(type);
	if (typeName.empty())
	{
		SetMetadataError(outError, "Component type name is not registered.");
		return false;
	}

	std::unique_ptr<Component> candidate(registry.Create(typeName));
	if (!candidate)
	{
		SetMetadataError(outError, "Component factory did not create a validation candidate.");
		return false;
	}

	GuidActorReferenceCodec actorCodec;
	AssetReferenceCodec assetCodec;
	ReflectionRestoreContext context{
		.actorReferenceCodec = &actorCodec,
		.assetReferenceCodec = &assetCodec,
	};

	if (!ReflectionDeserializer::Deserialize(
		*metadata, type, json, candidate.get(), context, outError))
	{
		return false;
	}

	return metadata->CopySerializableState(
		type, candidate.get(), &component, outError);
}
