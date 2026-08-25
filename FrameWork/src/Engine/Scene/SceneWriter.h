#pragma once
#include <string>
#include "nlohmann/json_fwd.hpp"

class SceneBase;

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
};
