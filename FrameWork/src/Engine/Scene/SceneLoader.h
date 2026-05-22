#pragma once
#include <string>

//-----------------------------------------------------------------------------
// SceneLoader class
// This class is responsible for loading scene data from a file.
//------------------------------------------------------------------------------

class SceneBase;
class EngineContext;

class SceneLoader
{
public:
	static bool Load(const std::string& filePath, SceneBase* scene, EngineContext& context);

private:
	static constexpr int CURRENT_SCENE_VERSION = 1;	// Current scene data version

	static void LoadActor(const nlohmann::json& actorJson, SceneBase* scene, Actor* parent);
};