#pragma once

#include "Engine/Core/Guid/Guid.h"

class Actor;
class SceneBase;

// GUID-backed selection owned by one Editor Document. It never keeps an
// Actor pointer across Scene replacement or Document activation.
class EditorSelection
{
public:
	void SelectActor(const Guid& actorGuid) { m_selectedActorGuid = actorGuid; }
	void Clear() { m_selectedActorGuid = {}; }

	Guid GetSelectedActorGuid() const { return m_selectedActorGuid; }
	Actor* ResolveActor(SceneBase* scene);
	void Revalidate(SceneBase* scene);

private:
	Guid m_selectedActorGuid;
};
