#pragma once
#include "nlohmann/json_fwd.hpp"
#include "Engine/Core/Reflection/ReflectionSerialization.h"

class Component;
class SceneBase;

struct ComponentRestoreOptions
{
	UnknownPropertyPolicy unknownPropertyPolicy = UnknownPropertyPolicy::Reject;
};

bool SerializeReflectedComponent(
	const Component& component,
	nlohmann::json& outJson,
	const SceneBase* scene = nullptr);

bool DeserializeReflectedComponent(
	Component& component,
	const nlohmann::json& json,
	ComponentRestoreOptions options = {});
