#pragma once
#include "Engine/Component/Behavior.h"

// Player class
class PlayerBehavior : public Behavior
{
public:	// Public functions
	PlayerBehavior() : Behavior() {}
	~PlayerBehavior() = default;

private:
	// Overridden functions
	void UpdateBehavior(float deltaTime) override;	// Scene-specific update
};