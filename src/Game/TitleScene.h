#pragma once
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"

// Title scene class
class TitleScene : public SceneBase
{
public:	
	TitleScene();	// Constructor
	~TitleScene();	// Destructor
private:
	void InitializeOverride(EngineContext& context) override {};	// Initialization (override in derived class)
};