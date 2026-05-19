#pragma once
#pragma once
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Component/Behavior.h"
#include "Engine/Core/Debug/Debug.h"

class TestBehavior : public Behavior
{
public:
    void Start() override
    {
        DBG("TestBehavior::Start()");
    }

    void Update() override
    {
        DBG("TestBehavior::Update()");
    }
};

REGISTER_BEHAVIOR(TestBehavior)