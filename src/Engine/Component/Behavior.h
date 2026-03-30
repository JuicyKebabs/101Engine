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
	void PreUpdate(float deltaTime) override { PreUpdateBehavior(deltaTime); }
	void Update(float deltaTime) override { UpdateBehavior(deltaTime); }
	void LateUpdate(float deltaTime) override { LateUpdateBehavior(deltaTime); }
	void OnDestroy() override { OnDestroyBehavior(); }

	virtual void OnStartBehavior() = 0;
	virtual void PreUpdateBehavior(float deltaTime) = 0;
	virtual void UpdateBehavior(float deltaTime) = 0;
	virtual void LateUpdateBehavior(float deltaTime) = 0;
	virtual void OnDestroyBehavior() = 0;
};