#include "TestBehavior.h"
#include "Engine/Actor/Actor.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Component/Transform.h"

REGISTER_GAME_COMPONENT(TestBehavior)

std::optional<TypeMetadata> TestBehavior::BuildMetadata()
{
	TypeMetadataBuilder<TestBehavior> builder("TestBehavior");
	builder.Property("rotationSpeed", &TestBehavior::m_rotationSpeed).Optional();
	builder.Property("actorImprint", &TestBehavior::m_actorImprint).Optional();
	return builder.Build();
}


void TestBehavior::Start()
{
    DBG("TestBehavior::Start()");
}

void TestBehavior::Update()
{
	const InputInfo& input = InputManager::GetInstance().GetInputInfo();

	if (input.key.a.down)
    {
        Transform* transform = GetOwner()->GetComponentByClass<Transform>();

        transform->RotateLocalByEulerDeg(Vector3(0.0f, 0.0f, m_rotationSpeed));
    }
	else if (input.key.d.down)
    {
        Transform* transform = GetOwner()->GetComponentByClass<Transform>();

        transform->RotateLocalByEulerDeg(Vector3(0.0f, 0.0f, -m_rotationSpeed));
    }

	if (!input.key.space.trigger)
	{
		return;
	}

	Actor* owner = GetOwner();
	SceneBase* scene = owner ? owner->GetOwner() : nullptr;
	EngineContext* context = owner ? GetEngineContext() : nullptr;
	Transform* sourceTransform = owner ? owner->GetComponentByClass<Transform>() : nullptr;
	if (!m_actorImprint.HasValue() || !scene || !context || !context->pActorImprintSystem || !sourceTransform)
	{
		DBG("TestBehavior: ActorImprint could not be instantiated because the reference or runtime context is invalid.");
		return;
	}

	const Vector3 spawnPosition = sourceTransform->GetWorldPosition();
	ActorImprintMaterializationError error;
	Actor* instanceRoot = context->pActorImprintSystem->Instantiate(*scene, m_actorImprint, {}, &error);
	if (!instanceRoot)
	{
		DBG("TestBehavior: ActorImprint instantiation failed: %s", error.message.c_str());
		return;
	}

	Transform* instanceTransform = instanceRoot->GetComponentByClass<Transform>();
	if (!instanceTransform)
	{
		context->pActorImprintSystem->DestroyInstance(*scene, instanceRoot->GetHandle());
		DBG("TestBehavior: ActorImprint instance root has no Transform.");
		return;
	}

	instanceTransform->SetLocalPosition(spawnPosition);
}
