#include "Command/EditorCommandHistory.h"
#include "Command/TransformEditCommand.h"

#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/UI/Canvas.h"

#include <iostream>
#include <memory>
#include <string>

namespace
{
	int g_failures = 0;

	void Check(bool condition, const std::string& name)
	{
		if (condition)
		{
			std::cout << "[PASS] " << name << '\n';
			return;
		}

		std::cerr << "[FAIL] " << name << '\n';
		++g_failures;
	}

	bool IsSameVector3(const Vector3& lhs, const Vector3& rhs)
	{
		return lhs.NearEqual(rhs);
	}

	bool IsSameTransform(const Transform3D& lhs, const Transform3D& rhs)
	{
		return IsSameVector3(lhs.position, rhs.position) &&
			lhs.rotation.NearEqual(rhs.rotation) &&
			IsSameVector3(lhs.scale, rhs.scale);
	}

	Actor* AddActor(SceneBase& scene, ActorType type, const char* name)
	{
		return scene.AddRootActor(ActorFactory::CreateActor(
			type,
			Actor::InitDesc(true, TAG_NONE, name)
		));
	}

	void TestExecuteUndoRedo()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* actor = AddActor(scene, ActorType::Mesh, "TransformActor");
		Transform* transform = actor->GetComponentByClass<Transform>();

		const Transform3D before{
			Vector3(1.0f, 2.0f, 3.0f),
			Quaternion::CreateFromEulerDeg(Vector3(10.0f, 20.0f, 30.0f)),
			Vector3(1.0f, 2.0f, 3.0f)
		};
		const Transform3D after{
			Vector3(4.0f, 5.0f, 6.0f),
			Quaternion::CreateFromEulerDeg(Vector3(40.0f, 50.0f, 60.0f)),
			Vector3(3.0f, 2.0f, 1.0f)
		};

		transform->SetLocalTransform(before);

		Check(history.Execute(std::make_unique<TransformEditCommand>(
			&scene, actor->GetGuid(), before, after)),
			"TransformEditCommand executes through command history");
		Check(IsSameTransform(transform->GetLocalTransform(), after),
			"Execute applies the complete after transform");
		Check(history.Undo() && IsSameTransform(transform->GetLocalTransform(), before),
			"Undo restores the complete before transform");
		Check(history.Redo() && IsSameTransform(transform->GetLocalTransform(), after),
			"Redo reapplies the complete after transform");
	}

	void TestInvalidTargetsFailSafely()
	{
		SceneBase scene;
		EditorCommandHistory history;
		const Transform3D state{};

		Check(!history.Execute(std::make_unique<TransformEditCommand>(
			nullptr, GuidGenerator::Generate(), state, state)),
			"Transform command rejects a null Scene");
		Check(!history.Execute(std::make_unique<TransformEditCommand>(
			&scene, GuidGenerator::Generate(), state, state)),
			"Transform command rejects an unresolved Actor Guid");
		Check(history.GetUndoCount() == 0,
			"Rejected Transform commands do not enter history");
	}

	void TestRectTransformIsNotEditedAsTransform()
	{
		SceneBase scene;
		EditorCommandHistory history;

		ComponentRegistry::Get().RegisterPolicy(
			std::type_index(typeid(RectTransform)),
			{
				ComponentCardinality::UniqueRequired,
				ComponentFamily::Transform
			}
		);
		ComponentRegistry::Get().RegisterPolicy(
			std::type_index(typeid(Canvas)),
			{
				ComponentCardinality::UniqueOptional,
				ComponentFamily::None
			}
		);

		auto actorInstance = ActorFactory::RestoreActorShell(
			Actor::InitDesc(true, TAG_NONE, "RectActor"),
			GuidGenerator::Generate()
		);
		actorInstance->AddComponent<RectTransform>();
		actorInstance->AddComponent<Canvas>();
		Actor* actor = scene.AddRootActor(std::move(actorInstance));
		const Transform3D state{};

		Check(actor->GetComponentByClass<RectTransform>() != nullptr,
			"RectTransform test Actor contains a RectTransform");
		Check(!history.Execute(std::make_unique<TransformEditCommand>(
			&scene, actor->GetGuid(), state, state)),
			"Transform command does not treat RectTransform as a normal Transform");
		Check(history.GetUndoCount() == 0,
			"Rejected RectTransform command does not enter history");
	}
}

int main()
{
	TestExecuteUndoRedo();
	TestInvalidTargetsFailSafely();
	TestRectTransformIsNotEditedAsTransform();

	if (g_failures == 0)
	{
		std::cout << "All TransformEditCommand tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " TransformEditCommand test(s) failed.\n";
	return 1;
}
