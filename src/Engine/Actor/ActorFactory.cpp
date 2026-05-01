#include "ActorFactory.h"

const std::unordered_map<ActorType, std::vector<std::function<void(Actor*)>>> ActorFactory::s_actorComponentMap = {
	{ ActorType::Empty, {} },
	{ ActorType::Mesh, { Adder<Transform>(), Adder<MeshRenderer>() } },
	{ ActorType::Sprite, { Adder<Transform>(), Adder<SpriteRenderer>() } },
	{ ActorType::UI, { Adder<RectTransform>() } },
	{ ActorType::Canvas, { Adder<RectTransform>() } },
	{ ActorType::Camera, { Adder<Transform>(), Adder<Camera>() } },
};