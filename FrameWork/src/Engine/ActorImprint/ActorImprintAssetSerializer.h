#pragma once
#include "nlohmann/json.hpp"

class ActorImprint;

class ActorImprintAssetSerializer
{
public:
	static nlohmann::json Serialize(const ActorImprint& imprint);
};

