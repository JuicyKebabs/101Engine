#pragma once
#include <vector>
#include "Engine/Component/TransformConversion.h"
#include "Engine/Core/GUID/Guid.h"

class Actor;
class SceneBase;

//------------------------------------------------------------------------------------------
// TransformSubtreeSnapshot class
// Store the exact Transform-family state of an Actor subtree
// It restores Transform and RectTransform instances without recreating their owning Actors
//------------------------------------------------------------------------------------------

class TransformSubtreeSnapshot
{

public:
	bool Capture(Actor* rootActor, SceneBase* scene);

	bool Restore(SceneBase* scene) const;

	void Clear();
	
	bool IsValid() const { return m_rootActorId.IsValid() && !m_records.empty(); }

	const Guid& GetRootActorId() const { return m_rootActorId; }

private:

	// Struct to hold the Actor Id and its corresponding TransformSnapshot
	struct Record
	{
		Guid actorId;
		TransformSnapshot transform;
	};

	// The Id of the root Actor of the captured subtree
	Guid m_rootActorId;

	// Records of each Actor in the captured subtree
	std::vector<Record> m_records;
};
