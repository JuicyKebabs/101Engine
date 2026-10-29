#pragma once
#include "Engine/Actor/Actor.h"

// Scene update/transform flushing skips inactive root subtrees. Extraction must
// use that same semantic membership: a child whose own flag is true still has
// no current World while an ancestor is inactive. This is Engine meaning, not
// Camera visibility, and must be resolved before submitting Renderer values.
inline bool IsActorHierarchyActive(const Actor* actor)
{
    if (!actor)
    {
        return false;
    }

    for (const Actor* current = actor; current; current = current->GetParent())
    {
        if (!current->IsActive() || current->IsDestroyed())
        {
            return false;
        }
    }

    return true;
}
