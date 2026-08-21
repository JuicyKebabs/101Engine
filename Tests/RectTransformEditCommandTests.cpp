#include "Command/EditorCommandHistory.h"
#include "Command/RectTransformEditCommand.h"

#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/RectTransform.h"
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

	bool IsSameState(const RectTransform& rectTransform, const RectTransformEditState& state)
	{
		return rectTransform.GetAnchorMode() == state.anchorMode &&
			rectTransform.GetAnchoredPosition().NearEqual(state.anchoredPosition) &&
			rectTransform.GetPivot().NearEqual(state.pivot) &&
			rectTransform.GetSize().NearEqual(state.size) &&
			rectTransform.GetLocalRotationQuat().NearEqual(state.localRotation) &&
			rectTransform.GetLocalScale().NearEqual(state.localScale);
	}

	void RegisterRectTransformPolicies()
	{
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
	}

	Actor* AddRectTransformActor(SceneBase& scene)
	{
		RegisterRectTransformPolicies();

		auto actorInstance = ActorFactory::RestoreActorShell(
			Actor::InitDesc(true, TAG_NONE, "RectTransformActor"),
			GuidGenerator::Generate()
		);
		actorInstance->AddComponent<RectTransform>();
		actorInstance->AddComponent<Canvas>();

		return scene.AddRootActor(std::move(actorInstance));
	}

	void TestExecuteUndoRedo()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* actor = AddRectTransformActor(scene);
		RectTransform* rectTransform = actor->GetComponentByClass<RectTransform>();

		const RectTransformEditState before{
			AnchorMode::TopLeft,
			Vector2(10.0f, 20.0f),
			Vector2(0.25f, 0.75f),
			Vector2(100.0f, 200.0f),
			Quaternion::CreateFromEulerDeg(Vector3(10.0f, 20.0f, 30.0f)),
			Vector3(1.0f, 2.0f, 3.0f)
		};
		const RectTransformEditState after{
			AnchorMode::BottomRight,
			Vector2(-30.0f, 40.0f),
			Vector2(0.8f, 0.2f),
			Vector2(300.0f, 150.0f),
			Quaternion::CreateFromEulerDeg(Vector3(40.0f, 50.0f, 60.0f)),
			Vector3(3.0f, 2.0f, 1.0f)
		};

		rectTransform->SetAnchorMode(before.anchorMode);
		rectTransform->SetAnchoredPosition(before.anchoredPosition);
		rectTransform->SetPivot(before.pivot);
		rectTransform->SetSizeDelta(before.size);
		rectTransform->SetLocalRotationQuat(before.localRotation);
		rectTransform->SetLocalScale(before.localScale);

		Check(history.Execute(std::make_unique<RectTransformEditCommand>(
			&scene, actor->GetGuid(), before, after)),
			"RectTransformEditCommand executes through command history");
		Check(IsSameState(*rectTransform, after),
			"Execute applies the complete after RectTransform state");
		Check(history.Undo() && IsSameState(*rectTransform, before),
			"Undo restores the complete before RectTransform state");
		Check(history.Redo() && IsSameState(*rectTransform, after),
			"Redo reapplies the complete after RectTransform state");
	}

	void TestInvalidTargetsFailSafely()
	{
		SceneBase scene;
		EditorCommandHistory history;
		const RectTransformEditState state{};

		Check(!history.Execute(std::make_unique<RectTransformEditCommand>(
			nullptr, GuidGenerator::Generate(), state, state)),
			"RectTransform command rejects a null Scene");
		Check(!history.Execute(std::make_unique<RectTransformEditCommand>(
			&scene, GuidGenerator::Generate(), state, state)),
			"RectTransform command rejects an unresolved Actor Guid");
		Check(history.GetUndoCount() == 0,
			"Rejected RectTransform commands do not enter history");
	}

	void TestNormalTransformIsNotEdited()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* actor = scene.AddRootActor(ActorFactory::CreateActor(
			ActorType::Mesh,
			Actor::InitDesc(true, TAG_NONE, "TransformActor")
		));
		const RectTransformEditState state{};

		Check(!history.Execute(std::make_unique<RectTransformEditCommand>(
			&scene, actor->GetGuid(), state, state)),
			"RectTransform command does not treat Transform as RectTransform");
		Check(history.GetUndoCount() == 0,
			"Rejected Transform target does not enter command history");
	}
}

int main()
{
	TestExecuteUndoRedo();
	TestInvalidTargetsFailSafely();
	TestNormalTransformIsNotEdited();

	if (g_failures == 0)
	{
		std::cout << "All RectTransformEditCommand tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " RectTransformEditCommand test(s) failed.\n";
	return 1;
}
