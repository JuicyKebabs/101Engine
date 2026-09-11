#pragma once
#include "Engine/Component/Behavior.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Input/InputManager.h"

class TestBehavior : public Behavior
{
public:
    void Start() override;

    void Update() override;

	static std::optional<TypeMetadata> BuildMetadata();

private:
	float m_rotationSpeed = 1.0f;
};
