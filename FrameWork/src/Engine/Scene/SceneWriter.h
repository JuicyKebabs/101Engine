#pragma once
#include <string>
#include "nlohmann/json_fwd.hpp"

class SceneBase;
class ActorImprintSystem;

//-------------------------------------------------------------------------------------------------
// SceneWriter class
// This class is responsible for saving and scene data to scene files with the appropriate format.
//-------------------------------------------------------------------------------------------------

class SceneWriter
{
public:
	// Save the given scene to a file at the specified path.
	static bool SaveScene(const std::string& filePath, SceneBase* scene);

	// Create JSON data for the given scene without saving to a file.
	static bool SerializeScene(const SceneBase* scene, nlohmann::json& outJson);

private:
	friend class ActorImprintSystem;
	// Captures a live Edit Scene for an in-memory reload transaction. Missing
	// definitions already retained by the System and a camera-less Scene are valid
	// here even though an explicit Scene save must reject them.
	static bool SerializeReloadSnapshot(const SceneBase* scene, nlohmann::json& outJson);
	static bool SerializeSceneImpl(const SceneBase* scene, nlohmann::json& outJson, bool forReload);
};
