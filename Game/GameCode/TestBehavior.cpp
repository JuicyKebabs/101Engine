#include "TestBehavior.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Component/Transform.h"

void TestBehavior::Start()
{
    DBG("TestBehavior::Start()");
}

void TestBehavior::Update()
{

    if (InputManager::GetInstance().GetInputInfo().key.a.down)
    {
        Transform* transform = GetOwner()->GetComponentByClass<Transform>();

        transform->RotateLocalByEulerDeg(Vector3(0.0f, 0.0f, 5.0f));
    }
}
