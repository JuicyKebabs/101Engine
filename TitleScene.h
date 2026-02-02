#pragma once
#include "SceneBase.h"
#include "Actor.h"

// Title scene class
class TitleScene : public SceneBase
{
public:	
	TitleScene(float window_width, float window_height);	// Constructor
	~TitleScene();											// Destructor

	// Main processing functions
	void InitializeOverride(EngineContext& context) override;	// Initialization
	void UpdateOverride() override;								// Update
	void ResolveCollisions() override {};						// Post-collision processing
	void FinalizeOverride() override;							// Finalize

private:
};