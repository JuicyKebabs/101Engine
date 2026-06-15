#pragma once
#include "Engine/Scene/SceneBase.h"

class HierarchyPanel
{
public:
    void Render(SceneBase* scene);
    Actor* GetSelectedActor() const { return m_pSelectedActor; }

	// Clears the current selection.
	// Must be called when the selected actor is destroyed (e.g. hot-reload is performed).
    void ClearSelection() { m_pSelectedActor = nullptr; }

private:
    void RenderActorNode(Actor* actor);
    Actor* m_pSelectedActor = nullptr;
};
