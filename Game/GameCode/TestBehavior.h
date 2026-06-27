#pragma once
#include "Engine/Component/Behavior.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Input/InputManager.h"

class TestBehavior : public Behavior
{
public:
    void Start() override;

    void Update() override;
};

REGISTER_GAME_COMPONENT(TestBehavior)