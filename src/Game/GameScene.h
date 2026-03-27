#pragma once
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Utility/SharedStruct.h"

// Game scene class
class GameScene : public SceneBase
{
public:	// Public functions
	GameScene(float window_width, float window_height);	// Constructor
	~GameScene();										// Destructor
private:
	void InitializeOverride(EngineContext& context) override;	// Initialization (override in derived class)
};