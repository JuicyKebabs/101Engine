#pragma once
#include "nlohmann/json_fwd.hpp"
#include <string>

namespace ActorImprintDetail
{

// Internal asset-reader diagnostic. The path addresses the original input.
struct ObjectGraphError
{
	std::string path;
	std::string message;
};

// Validates only object identities and the closed Actor hierarchy in asset JSON.
// Revision, allowed fields, component types and properties belong to the asset
// deserializer's other phases. Success here does not mean the asset is valid.
// Does not create Actors, touch a Scene, or change the input.
bool ValidateObjectGraph(const nlohmann::json& asset, ObjectGraphError& outError);

}

