#pragma once
#include "nlohmann/json_fwd.hpp"

namespace ActorImprintDetail
{
// Validates only object identities and the closed Actor hierarchy in asset JSON.
// Revision, allowed fields, component types and properties belong to the asset
// deserializer's other phases. Success here does not mean the asset is valid.
// Does not create Actors, touch a Scene, or change the input.
bool ValidateObjectGraph(const nlohmann::json& asset);

}
