#include "Core/CanvasEditContext.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/UI/Canvas.h"

bool CanvasEditContext::OpenFromActor(Actor* actor)
{
	Canvas* canvas = FindClosestCanvas(actor);
	if(!canvas) return false;

	Actor* canvasActor = canvas->GetOwner();

	return canvasActor && m_canvasActor.Set(canvasActor);
}

Canvas* CanvasEditContext::ResolveCanvas(SceneBase& scene) const
{
	Actor* canvasActor = m_canvasActor.Resolve(scene);

	return canvasActor
		? canvasActor->GetComponentByClass<Canvas>()
		: nullptr;
}

void CanvasEditContext::Clear()
{
	m_canvasActor.Clear();
}

Canvas* CanvasEditContext::FindClosestCanvas(Actor* actor)
{
	if (!actor || actor->IsDestroyed()) return nullptr;

	// Traverse up the actor hierarchy to find the closest Canvas component
	for (Actor* current = actor; current; current = current->GetParent())
	{
		Canvas* canvas = current->GetComponentByClass<Canvas>();
		if (canvas) return canvas;
	}

	return nullptr;
}
