#pragma once
#include "Engine/Scene/SceneBase.h"

// Game scene class
class GameScene : public SceneBase
{
public:	// Public functions
	GameScene();	// Constructor
	~GameScene();	// Destructor
private:
	void InitializeOverride(EngineContext& context) override;	// Initialization (override in derived class)
};