#pragma once
#include "BehaviorComponent.h"

// Player class
class PlayerBehavior : public BehaviorComponent
{
public:	// Public functions
	PlayerBehavior(Actor* owner) : BehaviorComponent(owner) {}
	~PlayerBehavior() = default;

private:
	// Overridden functions
	void Update(float deltaTime) override;	// Scene-specific update
};