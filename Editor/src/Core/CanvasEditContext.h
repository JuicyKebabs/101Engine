#pragma once
#include "Engine/Actor/ActorReference.h"

class Actor;
class SceneBase;
class Canvas;

//---------------------------------------------------------------------------------
// CanvasEditContext class
// Holds the Canvas currently being edited in Canvas View.
// 
// Note : 
// Canvas View treats the selected Canvas as a UI group boundary.
// The UI elements directly owned by this Canvas can be edited in the Canvas View.
// Elements governed by nested Canvases may be displayed as previews, 
// but are edited through their own CanvasEditContext target.
//---------------------------------------------------------------------------------

class CanvasEditContext
{
public:
	// Open the closest Canvas containing the given Actor.
	// The Actor's own Canvas takes priority over ancestor Canvases.
	bool OpenFromActor(Actor* actor);

	// Resolve the current Canvas against the active Scene.
	Canvas* ResolveCanvas(SceneBase& scene) const;

	void Clear();
	bool HasTarget() const { return m_canvasActor.HasValue(); }

	static Canvas* FindClosestCanvas(Actor* actor);

private:
	ActorReference m_canvasActor;	// Reference to the Canvas Actor for editing
};
