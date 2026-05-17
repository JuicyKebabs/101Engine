#pragma once
#include "Engine/Component/Behavior.h"

class CameraTest : public Behavior
{
public:
	CameraTest() : Behavior() {}
	virtual ~CameraTest() = default;
	void OnStartBehavior() override;
	void PreUpdateBehavior() override {}
	void UpdateBehavior() override;
	void LateUpdateBehavior() override {}
	void OnDestroyBehavior() override {}

private:
	bool m_isMainCamera = false;
};