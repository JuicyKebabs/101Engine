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
	static Actor* CreateActor(ActorType type, Actor::InitDesc desc) {
		Actor* actor = new Actor();
		actor->Init(desc);
		auto it = s_actorComponentMap.find(type);
		assert(it != s_actorComponentMap.end() && "Actor type not supported");
		for(const auto& componentAdder : it->second) {
			componentAdder(actor);
		}
		return actor;
	}

	// Create an empty actor with only a Transform component (common for all actors)
	static Actor* CreateEmptyActor(Actor::InitDesc desc)
	{
		Actor* actor = new Actor();
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