#pragma once
#include "SceneBase.h"
#include "SharedStruct.h"

// Game scene class
class GameScene : public SceneBase
{
public:	// Public functions
	GameScene(float window_width, float window_height);	// Constructor
	~GameScene();										// Destructor

	// Main processing functions
	void InitializeOverride(EngineContext& engineContext) override;	// Initialization
	void UpdateOverride() override;									// Update
	void ResolveCollisions() override;								// Post-collision processing
	void FinalizeOverride() override;								// Finalization

private:
};