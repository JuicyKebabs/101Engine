#pragma once
#include "Component.h"
#include <vector>
#include <memory>

// BehaviorComponent Class
class Behavior : public Component
{
public:
	Behavior(const std::string& name = "Behavior") : Component(name) {}
	virtual ~Behavior() = default;

	void OnStart() override { OnStartBehavior(); }
	void PreUpdate(float deltaTime) override { PreUpdateBehavior(); }
	void Update(float deltaTime) override { UpdateBehavior(); }
	void LateUpdate(float deltaTime) override { LateUpdateBehavior(); }
	void OnDestroy() override { OnDestroyBehavior(); }

	virtual void OnStartBehavior() = 0;
	virtual void PreUpdateBehavior() = 0;
	virtual void UpdateBehavior() = 0;
	virtual void LateUpdateBehavior() = 0;
	virtual void OnDestroyBehavior() = 0;
};