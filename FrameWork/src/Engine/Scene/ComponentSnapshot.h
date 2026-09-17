#pragma once
#include <cstddef>
#include "Engine/Core/GUID/Guid.h"
#include "nlohmann/json.hpp"

class Component;
class Actor;
class SceneBase;

//-----------------------------------------------------------------------------
// ComponentSnapshot class
// This class captures the state of a component and its owning actor
// Used by the commnads of adding and removing components to support Undo/Redo
//-----------------------------------------------------------------------------

class ComponentSnapshot
{
public:
	bool Capture(Actor* actor, Component* component);
	Component* Restore(SceneBase* scene) const;

	bool IsValid() const { return m_isValid; }

private:
	Guid m_actorGuid;
	nlohmann::json m_componentRecord;
	std::size_t m_occurrenceIndex = 0;
	bool m_isValid = false;
};
