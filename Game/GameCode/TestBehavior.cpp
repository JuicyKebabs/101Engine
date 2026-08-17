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
    if (InputManager::GetInstance().GetInputInfo().key.a.trigger)
    {
		DBG("A key pressed, changing scene to 'output'");
        ChangeScene("output");

        Transform* transform = GetOwner()->GetComponentByClass<Transform>();

        float dt = TimeManager::GetInstance().GetDeltaTime();
        const float rotSpeed = 5.0f;

        transform->RotateLocalByEulerDeg(Vector3(0.0f, 0.0f, dt * 5.0f));
    }
}
