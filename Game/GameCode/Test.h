#pragma once
#include "Engine/Component/Behavior.h"

class Test : public Behavior
{
public:
    void Start() override;
    void PreUpdate() override;
    void Update() override;
    void LateUpdate() override;
    void Destroy() override;
};

REGISTER_GAME_COMPONENT(Test)