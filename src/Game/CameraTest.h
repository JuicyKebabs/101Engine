#pragma once
#include "Engine/Component/Behavior.h"

class CameraTest : public Behavior
{
public:
	CameraTest(const std::string& name = "CameraTest") : Behavior(name) {}
	virtual ~CameraTest() = default;
	void OnStartBehavior() override;
	void PreUpdateBehavior(float deltaTime) override {}
	void UpdateBehavior(float deltaTime) override;
	void LateUpdateBehavior(float deltaTime) override {}
	void OnDestroyBehavior() override {}

private:
	bool m_isMainCamera = false;
};