#pragma once
#include "Component.h"
#include <vector>
#include <memory>

// BehaviorComponent Class
class BehaviorComponent : public Component
{
public:
	BehaviorComponent(Actor* owner) : Component(owner) {}
	virtual ~BehaviorComponent() = default;

	virtual void Update(float deltaTime) = 0; // Pure virtual function for updating behavior
};