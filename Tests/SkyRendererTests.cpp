#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/SkyRenderer.h"
#include "Engine/Component/Transform.h"
#include "Engine/Graphics/FrameRenderData.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Scene/SceneBase.h"
#include "nlohmann/json.hpp"

#include <cmath>
#include <iostream>
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

	Actor* AddSkyActor(SceneBase& scene, const char* name, SkyRenderer*& sky)
	{
		auto actor = ActorFactory::CreateEmptyActor(Actor::InitDesc(true, TAG_NONE, name));
		sky = actor ? actor->AddComponent<SkyRenderer>() : nullptr;
		return actor ? scene.AddRootActor(std::move(actor)) : nullptr;
	}

	void TestActiveSkyLifetimeAndSceneValidation()
	{
		SceneBase scene;
		SceneBase otherScene;
		SkyRenderer* first = nullptr;
		SkyRenderer* second = nullptr;
		SkyRenderer* foreign = nullptr;
		Actor* firstActor = AddSkyActor(scene, "FirstSky", first);
		Actor* secondActor = AddSkyActor(scene, "SecondSky", second);
		AddSkyActor(otherScene, "ForeignSky", foreign);

		Check(first && first->SetAsActiveSkyRenderer(),
			"A SkyRenderer can become active in its owning Scene");
		Check(scene.GetRenderSystem()->GetActiveSkyRenderer() == first,
			"RenderSystem retains the selected SkyRenderer");
		Check(!scene.GetRenderSystem()->SetActiveSkyRenderer(foreign) &&
			scene.GetRenderSystem()->GetActiveSkyRenderer() == first,
			"RenderSystem rejects a SkyRenderer from another Scene");

		Check(second && second->SetAsActiveSkyRenderer(),
			"A later explicit selection replaces the active SkyRenderer");
		Check(scene.RemoveActorComponentImmediate(firstActor, first) &&
			scene.GetRenderSystem()->GetActiveSkyRenderer() == second,
			"Detaching an inactive SkyRenderer preserves the active selection");
		Check(scene.RemoveActorComponentImmediate(secondActor, second) &&
			scene.GetRenderSystem()->GetActiveSkyRenderer() == nullptr,
			"Detaching the active SkyRenderer clears the retained pointer");
	}

	bool Near(float left, float right)
	{
		return std::abs(left - right) < 0.0001f;
	}

	void TestOwnerFollowUsesDirtyPositionOnly()
	{
		SceneBase scene;
		SkyRenderer* sky = nullptr;
		Actor* actor = AddSkyActor(scene, "FollowingSky", sky);
		Transform* transform = actor ? actor->GetComponentByClass<Transform>() : nullptr;
		Check(sky && transform, "Sky test Actor has required components");
		if (!sky || !transform) return;

		transform->SetLocalPosition({ 1.0f, 2.0f, 3.0f });
		transform->SetLocalRotationEulerDeg({ 20.0f, 30.0f, 40.0f });
		transform->SetLocalScale({ 2.0f, 3.0f, 4.0f });
		transform->UpdateGeometry();
		scene.PreUpdate(0.0f);
		scene.Update(0.0f);
		const Vector3 first = sky->GetRenderProxy().common.position;
		transform->SetLocalPosition({ 4.0f, 5.0f, 6.0f });
		transform->UpdateGeometry();
		const Vector3 beforeUpdate = sky->GetRenderProxy().common.position;
		scene.Update(0.0f);
		const auto& proxy = sky->GetRenderProxy();
		const Vector3 second = sky->GetRenderProxy().common.position;
		Check(first.x == 1.0f && first.y == 2.0f && first.z == 3.0f &&
			beforeUpdate.x == 1.0f && beforeUpdate.y == 2.0f && beforeUpdate.z == 3.0f &&
			second.x == 4.0f && second.y == 5.0f && second.z == 6.0f,
			"Owner follow rebuilds only after Update observes a generation change");
		const Vector3 scale = proxy.common.worldMatrix.GetScale();
		Check(Near(scale.x, scale.y) && Near(scale.y, scale.z) &&
			Near(proxy.common.worldMatrix.GetTranslation().x, 4.0f),
			"Sky proxy ignores target rotation and non-uniform scale");
	}

	void TestMissingTargetsFallBackToOwner()
	{
		SceneBase scene;
		SkyRenderer* sky = nullptr;
		AddSkyActor(scene, "FallbackSky", sky);
		Check(sky != nullptr, "Fallback test creates a SkyRenderer");
		if (!sky) return;
		scene.PreUpdate(0.0f);

		sky->SetFollowMode(SkyRenderer::FollowMode::MainCamera);
		scene.Update(0.0f);
		Check(sky->GetFollowMode() == SkyRenderer::FollowMode::Owner,
			"Missing MainCamera falls back to Owner follow");

		const Guid missing = GuidGenerator::Generate();
		sky->SetFollowActor(missing);
		sky->SetFollowMode(SkyRenderer::FollowMode::Actor);
		scene.Update(0.0f);
		Check(sky->GetFollowMode() == SkyRenderer::FollowMode::Owner &&
			sky->GetFollowActorReference().GetGuid() == missing,
			"Missing Actor falls back without discarding its persistent Guid");
	}

	void TestSerializationRoundTrip()
	{
		const Guid texture = GuidGenerator::Generate();
		const Guid actor = GuidGenerator::Generate();
		SkyRenderer source;
		source.SetBlendMode(BlendMode::Multiply);
		nlohmann::json data;
		Check(source.Serialize(data), "SkyRenderer default state serializes");
		data["name"] = "SavedSky";
		data["color"] = { 0.25f, 0.5f, 0.75f, 1.0f };
		data["visible"] = false;
		data["skyTextureAssetId"] = texture.ToString();
		data["followMode"] = static_cast<int>(SkyRenderer::FollowMode::Actor);
		data["followActorId"] = actor.ToString();

		SkyRenderer restored;
		Check(restored.Deserialize(data) && restored.GetName() == "SavedSky" &&
			!restored.GetVisible() && restored.GetSkyTextureAssetId() == texture &&
			restored.GetBlendMode() == BlendMode::Multiply &&
			restored.GetFollowMode() == SkyRenderer::FollowMode::Actor &&
			restored.GetFollowActorReference().GetGuid() == actor,
			"SkyRenderer restores common, Asset and Actor reference state");
		data["followMode"] = 999;
		Check(!restored.Deserialize(data), "SkyRenderer rejects an invalid FollowMode");
	}

	void TestFrameRenderDataClearResetsSky()
	{
		FrameRenderData data;
		data.sky = RenderItemRef{ RenderType::Mesh, 0, 0 };
		data.AddMeshs({});
		data.Clear();
		Check(!data.sky.has_value() && data.GetMeshCount() == 0,
			"FrameRenderData clears Sky and mesh storage together");
	}
}

int main()
{
	TestActiveSkyLifetimeAndSceneValidation();
	TestOwnerFollowUsesDirtyPositionOnly();
	TestMissingTargetsFallBackToOwner();
	TestSerializationRoundTrip();
	TestFrameRenderDataClearResetsSky();

	if (g_failures == 0)
	{
		std::cout << "All SkyRenderer tests passed.\n";
		return 0;
	}
	std::cerr << g_failures << " SkyRenderer test(s) failed.\n";
	return 1;
}
