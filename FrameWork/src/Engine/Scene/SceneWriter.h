#pragma once
#include <string>
//-------------------------------------------------------------------------------------------------
// SceneWriter class
// This class is responsible for saving and scene data to scene files with the appropriate format.
//-------------------------------------------------------------------------------------------------

class SceneBase;

class SceneWriter
{
public:
	// Save the given scene to a file at the specified path.
	static bool SaveScene(const std::string& filePath, SceneBase* scene);

private:
	static constexpr int CURRENT_VERSION = 1; // Current version of the scene file format. Increment this when the format changes to maintain backward compatibility.
};