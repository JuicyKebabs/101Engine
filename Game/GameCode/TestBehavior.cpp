#include "TestBehavior.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Component/Transform.h"

namespace
{
	const bool registered = []
	{
		auto metadata = TestBehavior::BuildMetadata();
		if (!metadata) return false;
		ComponentRegistry::Get().RegisterGameComponent(
			"TestBehavior",
			[] { return static_cast<Component*>(new TestBehavior()); },
			std::type_index(typeid(TestBehavior)),
			std::make_unique<TypeMetadata>(std::move(*metadata)));
		return true;
	}();
}

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
