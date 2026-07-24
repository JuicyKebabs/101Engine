#pragma once
#include <string>
#include "SceneVersion.h"
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
};
