#pragma once
#include "Engine/Scene/SceneBase.h"
#include "TestBehavior.h"

#include "nlohmann/json.hpp"

// Sample scene class (for testing)
class SampleScene : public SceneBase
{
public:
	SampleScene() = default;	// Constructor
	~SampleScene() = default;	// Destructor

private:
	void InitializeOverride(EngineContext& context) override {
		if (ComponentRegistry::Get().Has("TestBehavior"))
		{
			DBG("TestBehavior is registered!");
		}
		else
		{
			DBG("TestBehavior is NOT registered.");
		}


		nlohmann::json j;
		j["version"] = 1;
		j["name"] = "test";
		std::string str = j.dump(4); // êÆå`èoóÕ
		DBG("%s", str.c_str());

	};	// Initialization (override)
};