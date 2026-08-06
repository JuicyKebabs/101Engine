#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Component/Transform.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/UI/Canvas.h"
#include "Engine/UI/UIImage.h"

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

	Actor* AddRoot(SceneBase& scene, const char* name)
	{
		return scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, name)));
	}

	Actor* AddChild(SceneBase& scene, Actor* parent, const char* name)
	{
		return scene.AddChildActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, name)),
			parent->GetHandle());
	}

	void TestAddingCanvasReappliesConstraintsToExistingSubtree()
	{
		SceneBase scene;
		Actor* root = AddRoot(scene, "Root");
		Actor* child = AddChild(scene, root, "Child");

		Check(root->GetComponentByClass<RectTransform>() == nullptr &&
			child->GetComponentByClass<RectTransform>() == nullptr,
			"A normal hierarchy starts with Transform components");

		Canvas* canvas = root->AddComponent<Canvas>();

		Check(canvas != nullptr,
			"Canvas can be added to an Actor already registered in a Scene");
		Check(canvas &&
			canvas->GetAuthoredRenderMode() == CanvasRenderMode::ScreenSpace &&
			canvas->GetRenderMode() == CanvasRenderMode::ScreenSpace,
			"A newly added Canvas starts with Screen-Space authored and effective modes");
		Check(root->GetComponentByClass<RectTransform>() != nullptr,
			"Adding a Screen-Space Canvas converts its owner to RectTransform immediately");
		Check(child->GetComponentByClass<RectTransform>() != nullptr,
			"Adding a Canvas reapplies RectTransform constraints to existing descendants");
	}

	void TestUIRendererAutomaticallyFollowsCanvasHierarchy()
	{
		SceneBase scene;
		Actor* root = AddRoot(scene, "Root");
		Actor* imageActor = AddChild(scene, root, "Image");
		UIImage* image = imageActor->AddComponent<UIImage>();

		Check(image && image->GetCanvas() == nullptr && !image->IsVisible(),
			"A UIImage outside a Canvas hierarchy remains unbound and invisible");

		Canvas* canvas = root->AddComponent<Canvas>();
		Check(canvas && image->GetCanvas() == canvas,
			"Adding Canvas binds existing descendant UIImage components automatically");

		scene.PreUpdate(0.0f);
		Check(image->IsStarted() && image->IsVisible(),
			"A bound UIImage starts and becomes eligible for rendering");

		Check(scene.ReparentActor(imageActor, nullptr),
			"UIImage Actor can leave its Canvas hierarchy");
		Check(image->GetCanvas() == nullptr && !image->IsVisible() &&
			imageActor->GetComponentByClass<RectTransform>() == nullptr,
			"Leaving Canvas clears the binding, visibility, and RectTransform constraint");

		Check(scene.ReparentActor(imageActor, root),
			"UIImage Actor can re-enter its Canvas hierarchy");
		Check(image->GetCanvas() == canvas && image->IsVisible() &&
			imageActor->GetComponentByClass<RectTransform>() != nullptr,
			"Re-entering Canvas restores binding, visibility, and RectTransform constraint");
	}

	void TestNestedCanvasKeepsAuthoredMode()
	{
		SceneBase scene;

		Actor* screenCanvasActor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "ScreenCanvas")));

		auto worldCanvasOwned = ActorFactory::CreateActor(
			ActorType::Canvas,
			Actor::InitDesc(true, TAG_NONE, "WorldCanvas"));
		Canvas* worldCanvas = worldCanvasOwned->GetComponentByClass<Canvas>();
		Canvas::ParamDesc desc;
		desc.renderMode = CanvasRenderMode::WorldSpace;
		worldCanvas->SetParams(desc);
		Actor* worldCanvasActor = scene.AddRootActor(std::move(worldCanvasOwned));

		Check(worldCanvasActor->GetComponentByClass<RectTransform>() == nullptr &&
			worldCanvasActor->GetComponentByClass<Transform>() != nullptr,
			"A root World-Space Canvas uses a normal Transform");

		Check(scene.ReparentActor(worldCanvasActor, screenCanvasActor),
			"World-Space Canvas can be nested under Screen-Space Canvas");
		Check(worldCanvas->GetAuthoredRenderMode() == CanvasRenderMode::WorldSpace &&
			worldCanvas->GetRenderMode() == CanvasRenderMode::ScreenSpace,
			"Nested Canvas inherits Screen-Space without overwriting authored World-Space");
		Check(worldCanvasActor->GetComponentByClass<RectTransform>() != nullptr,
			"Nested Canvas uses RectTransform from its effective hierarchy mode");

		nlohmann::json canvasJson;
		Check(worldCanvas->Serialize(canvasJson) &&
			canvasJson["renderMode"].get<int>() ==
			static_cast<int>(CanvasRenderMode::WorldSpace),
			"Canvas serialization stores authored mode instead of inherited effective mode");

		Check(scene.ReparentActor(worldCanvasActor, nullptr),
			"Nested Canvas can return to the scene root");
		Check(worldCanvas->GetRenderMode() == CanvasRenderMode::WorldSpace &&
			worldCanvasActor->GetComponentByClass<RectTransform>() == nullptr,
			"Returning to root restores authored mode and normal Transform");
	}

	void TestRendererFamilyAutomaticallyFollowsCanvasHierarchy()
	{
		SceneBase scene;
		Actor* root = AddRoot(scene, "RendererRoot");
		Actor* meshActor = AddChild(scene, root, "Mesh");
		Actor* spriteActor = AddChild(scene, root, "Sprite");
		MeshRenderer* mesh = meshActor->AddComponent<MeshRenderer>();

		Check(mesh && mesh->GetGoverningCanvas() == nullptr &&
			mesh->GetRenderSpace() == RenderSpace::World,
			"MeshRenderer outside Canvas starts in World render space");

		Canvas* canvas = root->AddComponent<Canvas>();
		Check(canvas && mesh->GetGoverningCanvas() == canvas &&
			mesh->GetRenderSpace() == RenderSpace::Screen,
			"Adding Canvas binds an existing MeshRenderer to Screen render space");

		SpriteRenderer* sprite = spriteActor->AddComponent<SpriteRenderer>();
		Check(sprite && sprite->GetGoverningCanvas() == canvas &&
			sprite->GetRenderSpace() == RenderSpace::Screen,
			"Adding SpriteRenderer under Canvas binds it immediately");

		Check(scene.ReparentActor(meshActor, nullptr) &&
			mesh->GetGoverningCanvas() == nullptr &&
			mesh->GetRenderSpace() == RenderSpace::World,
			"Leaving Canvas restores MeshRenderer World render space");

		Check(scene.ReparentActor(meshActor, root) &&
			mesh->GetGoverningCanvas() == canvas &&
			mesh->GetRenderSpace() == RenderSpace::Screen,
			"Re-entering Canvas restores MeshRenderer Screen render space");
	}
}

int main()
{
	TestAddingCanvasReappliesConstraintsToExistingSubtree();
	TestUIRendererAutomaticallyFollowsCanvasHierarchy();
	TestNestedCanvasKeepsAuthoredMode();
	TestRendererFamilyAutomaticallyFollowsCanvasHierarchy();

	if (g_failures == 0)
	{
		std::cout << "All UI hierarchy constraint tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " UI hierarchy constraint test(s) failed.\n";
	return 1;
}
