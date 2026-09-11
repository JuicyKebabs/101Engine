#pragma once

#include "nlohmann/json_fwd.hpp"

class Component;
class SceneBase;
struct ReflectionError;

bool SerializeReflectedComponent(
	const Component& component,
	nlohmann::json& outJson,
	const SceneBase* scene = nullptr,
	ReflectionError* outError = nullptr);

bool DeserializeReflectedComponent(
	Component& component,
	const nlohmann::json& json,
	ReflectionError* outError = nullptr);
