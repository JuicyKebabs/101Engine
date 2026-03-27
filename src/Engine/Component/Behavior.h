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

	void OnStart() override {}
	void PreUpdate(float deltaTime) override {}
	void Update(float deltaTime) override { UpdateBehavior(deltaTime); }
	void LateUpdate(float deltaTime) override {}
	void OnDestroy() override {}

	virtual void UpdateBehavior(float deltaTime) = 0;
};