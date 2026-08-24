#pragma once
#include <memory>
#include "Engine/Core/Context/Context.h"

class SceneBase;

//----------------------------------------------------------------------------
// SceneCloner class
// Tool for cloning a SceneBase instance by serializing and deserializing it.
// Used in the editor to create a runtime copy of the scene for Play mode.
//----------------------------------------------------------------------------

class SceneCloner
{
public:
	static std::unique_ptr<SceneBase> Clone(const SceneBase* scene, EngineContext& context);
};