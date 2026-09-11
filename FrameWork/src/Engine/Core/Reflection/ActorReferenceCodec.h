#pragma once
#include "Engine/Actor/ActorReference.h"
#include "nlohmann/json_fwd.hpp"

class Actor;

//---------------------------------------------------------------------------------
// ActorReferenceCodec
// This interface allows ActorReference to be serialized and deserialized to JSON.

enum class ActorReferenceCodecResult
{
	Success,
	InvalidJsonType,
	InvalidGuid,
	ActorNotFound,
	PendingDestroy,
};

// Interface for saving ActorReference to JSON. 
// Implement this interface to provide context for validating ActorReferences during serialization.
class ActorReferenceSaveContext
{
public:
	virtual ~ActorReferenceSaveContext() = default;
	virtual ActorReferenceCodecResult Validate(const Guid& guid) const = 0;
};

// Interface for restoring ActorReference from JSON.
// Implement this interface to provide context for resolving ActorReferences during deserialization.
class ActorReferenceRestoreContext
{
public:
	virtual ~ActorReferenceRestoreContext() = default;
	virtual ActorReferenceCodecResult FindActor(const Guid& guid, Actor*& outActor) const = 0;
};

// Base class to provide common interface for serializing 
// and deserializing ActorReference to/from JSON.
class ActorReferenceCodec
{
public:
	virtual ~ActorReferenceCodec() = default;

	virtual ActorReferenceCodecResult Serialize(
		const ActorReference& reference,
		const ActorReferenceSaveContext& context,
		nlohmann::json& outJson) const = 0;

	virtual ActorReferenceCodecResult Deserialize(
		const nlohmann::json& json,
		ActorReference& outReference) const = 0;

	virtual ActorReferenceCodecResult Resolve(
		ActorReference& reference,
		const ActorReferenceRestoreContext& context) const = 0;
};

// Concrete implementation of ActorReferenceCodec that serializes ActorReference as a Guid string in JSON.
class GuidActorReferenceCodec final : public ActorReferenceCodec
{
public:
	ActorReferenceCodecResult Serialize(
		const ActorReference& reference,
		const ActorReferenceSaveContext& context,
		nlohmann::json& outJson) const override;

	ActorReferenceCodecResult Deserialize(
		const nlohmann::json& json,
		ActorReference& outReference) const override;

	ActorReferenceCodecResult Resolve(
		ActorReference& reference,
		const ActorReferenceRestoreContext& context) const override;
};
