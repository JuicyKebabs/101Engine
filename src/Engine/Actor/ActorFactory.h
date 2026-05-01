#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <assert.h>
#include "Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Component/Camera.h"

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

private:
	template<typename T>
	static std::function<void(Actor*)> Adder() { return [](Actor* actor) { actor->AddComponent<T>(); }; }
	static const std::unordered_map<ActorType, std::vector<std::function<void(Actor*)>>> s_actorComponentMap;
};