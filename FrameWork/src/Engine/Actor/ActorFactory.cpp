#include "ActorFactory.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Component/Camera.h"
#include "Engine/UI/Canvas.h"
#include "Engine/UI/UIImage.h"
#include "Engine/Core/GUID/GuidGenerator.h"

const std::unordered_map<ActorType, std::vector<std::function<void(Actor*)>>> ActorFactory::s_actorComponentMap = {
	{ ActorType::Empty,  {} },
	{ ActorType::Mesh,   { Adder<Transform>(), Adder<MeshRenderer>() } },
	{ ActorType::Sprite, { Adder<Transform>(), Adder<SpriteRenderer>() } },
	{ ActorType::UI,     { Adder<RectTransform>(), Adder<UIImage>() } },
	{ ActorType::Canvas, { Adder<RectTransform>(), Adder<Canvas>() } },
	{ ActorType::Camera, { Adder<Transform>(), Adder<Camera>() } },
};

std::unique_ptr<Actor> ActorFactory::CreateActor(
	ActorType type, 
	const Actor::InitDesc& desc
)
{
	// Create actor with new generated Guid
	return CreateActorInternal(type, desc, GuidGenerator::Generate());
}

std::unique_ptr<Actor> ActorFactory::RestoreActor(
	ActorType type, 
	const Actor::InitDesc& desc, 
	const Guid& guid
)
{
	// Check if the provided Guid is valid before restoring the actor
	assert(guid.IsValid() && "RestoreActor: Guid must be valid");
	if (!guid.IsValid()) return nullptr;

	// Create actor with existing Guid
	return CreateActorInternal(type, desc, guid);
}

std::unique_ptr<Actor> ActorFactory::CreateActorInternal(
	ActorType type,
	const Actor::InitDesc& desc,
	const Guid& guid
)
{
	// Create a new actor and initialize it
	auto actor = std::unique_ptr<Actor>(new Actor());
	actor->Init(desc);
	actor->SetGuid(guid);

	// Get all function objects for adding necessary components based on the actor type
	auto it = s_actorComponentMap.find(type);
	assert(it != s_actorComponentMap.end() && "Actor type not supported");

	// Return nullptr for unsupported actor types (not found in the map)
	if (it == s_actorComponentMap.end())
	{
		return nullptr;
	}

	// Add all necessary components to the actor
	for (const auto& componentAdder : it->second)
	{
		componentAdder(actor.get());
	}

	return actor;
}

std::unique_ptr<Actor> ActorFactory::CreateEmptyActor(
	const Actor::InitDesc& desc
)
{
	// Create an empty actor with a new generated Guid
	return CreateEmptyActorInternal(desc, GuidGenerator::Generate());
}

std::unique_ptr<Actor> ActorFactory::RestoreEmptyActor(
	const Actor::InitDesc& desc,
	const Guid& guid
)
{
	// Check if the provided Guid is valid before restoring the actor
	assert(guid.IsValid() && "RestoreEmptyActor: Guid must be valid");
	if (!guid.IsValid()) return nullptr;

	// Restore an empty actor with the provided Guid
	return CreateEmptyActorInternal(desc, guid);
}

std::unique_ptr<Actor> ActorFactory::RestoreActorShell(
	const Actor::InitDesc& desc,
	const Guid& guid)
{
	assert(guid.IsValid() && "RestoreActorShell: Guid must be valid");

	if (!guid.IsValid()) return nullptr;

	auto actor = std::unique_ptr<Actor>(new Actor());

	actor->Init(desc);
	actor->SetGuid(guid);

	return actor;
}

std::unique_ptr<Actor> ActorFactory::CreateEmptyActorInternal(
	const Actor::InitDesc& desc,
	const Guid& guid
)
{
	// Create actor with the provided Guid and add a Transform component
	auto actor = std::unique_ptr<Actor>(new Actor());
	actor->Init(desc);
	actor->SetGuid(guid);
	actor->AddComponent<Transform>();

	return actor;
}