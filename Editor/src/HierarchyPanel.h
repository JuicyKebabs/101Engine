#pragma once
#include "Engine/Scene/SceneBase.h"

class HierarchyPanel
{
public:
    void Render(SceneBase* scene);
    Actor* GetSelectedActor() const { return m_pSelectedActor; }

private:
    void RenderActorNode(Actor* actor);
    Actor* m_pSelectedActor = nullptr;
};
