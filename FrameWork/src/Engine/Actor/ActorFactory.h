#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <assert.h>
#include "Actor.h"

//-----------------------------------------------------------------------------------------------------------------------------------
// ActorFactory class
// This factory class provides static methods to create actors with predefined sets of components based on the specified actor type.
//-----------------------------------------------------------------------------------------------------------------------------------

// Pre-defined actor types enumration
enum class ActorType
{
	Empty,
	Mesh,
	Sprite,
	UI,
	Canvas,
	Camera,
};

class ActorFactory
{
public:
	ActorFactory() = default;
	~ActorFactory() = default;

	// Create an actor of the specified type with the given initialization parameters
	// NOTE: Implementation moved to ActorFactory.cpp.
	// Reason: when this was an inline function defined in the header, callers in
	// 101Editor.exe inlined a direct reference to the private static
	// s_actorComponentMap data symbol, whose definition lives inside
	// 101Framework.dll. WINDOWS_EXPORT_ALL_SYMBOLS does not reliably export
	// private static const data members, causing LNK2001 in 101Editor.
	// Keeping the implementation in the .cpp confines all references to
	// s_actorComponentMap to 101Framework.dll itself.
	static std::unique_ptr<Actor> CreateActor(ActorType type, Actor::InitDesc desc);

	// Create an empty actor with only a Transform component (common for all actors)
	static std::unique_ptr<Actor> CreateEmptyActor(Actor::InitDesc desc)
	{
		auto actor = std::unique_ptr<Actor>(new Actor());
		actor->Init(desc);
		actor->AddComponent<Transform>();
		return actor;
	}

private:
	// Adding component of type T to the actor
	template<typename T>
	static std::function<void(Actor*)> Adder() { return [](Actor* actor) { actor->AddComponent<T>(); }; }

	// Map of actor types to their necessary components.
	static const std::unordered_map<ActorType, std::vector<std::function<void(Actor*)>>> s_actorComponentMap;
};
