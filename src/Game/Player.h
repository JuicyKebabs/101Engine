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
	void PreUpdateBehavior() override {}
	void UpdateBehavior() override;
	void LateUpdateBehavior() override;
	void OnDestroyBehavior() override {}
};