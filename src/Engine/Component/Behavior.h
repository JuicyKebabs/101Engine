#pragma once
#include "Component.h"
#include <vector>
#include <memory>

// BehaviorComponent Class
class Behavior : public Component
{
public:
	Behavior() = default;
	virtual ~Behavior() = default;

	void OnStartOverride() override { OnStartBehavior(); }
	void PreUpdateOverride(float deltaTime) override { PreUpdateBehavior(); }
	void UpdateOverride(float deltaTime) override { UpdateBehavior(); }
	void LateUpdateOverride(float deltaTime) override { LateUpdateBehavior(); }
	void OnDestroyOverride() override { OnDestroyBehavior(); }

	virtual void OnStartBehavior() = 0;
	virtual void PreUpdateBehavior() = 0;
	virtual void UpdateBehavior() = 0;
	virtual void LateUpdateBehavior() = 0;
	virtual void OnDestroyBehavior() = 0;
};