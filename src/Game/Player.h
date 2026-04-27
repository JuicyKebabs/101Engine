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
	void OnStartBehavior() override {}
	void PreUpdateBehavior(float deltaTime) override {}
	void UpdateBehavior(float deltaTime) override;
	void LateUpdateBehavior(float deltaTime) override;
	void OnDestroyBehavior() override {}
};