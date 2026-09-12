#include "TestBehavior.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Component/Transform.h"

REGISTER_GAME_COMPONENT(TestBehavior)

std::optional<TypeMetadata> TestBehavior::BuildMetadata()
{
	TypeMetadataBuilder<TestBehavior> builder("TestBehavior");
	builder.Property("rotationSpeed", &TestBehavior::m_rotationSpeed);
	return builder.Build();
}


void TestBehavior::Start()
{
    DBG("TestBehavior::Start()");
}

void TestBehavior::Update()
{

    if (InputManager::GetInstance().GetInputInfo().key.a.down)
    {
        Transform* transform = GetOwner()->GetComponentByClass<Transform>();

        transform->RotateLocalByEulerDeg(Vector3(0.0f, 0.0f, m_rotationSpeed));
    }
    else if (InputManager::GetInstance().GetInputInfo().key.d.down)
    {
        Transform* transform = GetOwner()->GetComponentByClass<Transform>();

        transform->RotateLocalByEulerDeg(Vector3(0.0f, 0.0f, -m_rotationSpeed));
    }
}
