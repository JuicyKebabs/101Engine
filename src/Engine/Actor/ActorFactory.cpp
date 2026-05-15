#include "ActorFactory.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Component/Camera.h"
#include "Engine/UI/Canvas.h"
#include "Engine/UI/UIImage.h"

const std::unordered_map<ActorType, std::vector<std::function<void(Actor*)>>> ActorFactory::s_actorComponentMap = {
	{ ActorType::Empty, {} },
	{ ActorType::Mesh, { Adder<Transform>(), Adder<MeshRenderer>() } },
	{ ActorType::Sprite, { Adder<Transform>(), Adder<SpriteRenderer>() } },
	{ ActorType::UI,     { Adder<RectTransform>(), Adder<UIImage>() }},
	{ ActorType::Canvas, { Adder<RectTransform>(), Adder<Canvas>() }},
	{ ActorType::Camera, { Adder<Transform>(), Adder<Camera>() } },
};