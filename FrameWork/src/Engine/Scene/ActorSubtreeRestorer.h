#pragma once

class Actor;
class ActorSubtreeSnapshot;
class SceneBase;

//--------------------------------------------------------------------------------------------
// ActorSubtreeRestorer class
// This class restores an Actor subtree into an existing Scene from ActorSubtreeSnapshot.
// If restoration fails after registration begins, every newly registered Actor is rolled back.
//--------------------------------------------------------------------------------------------

class ActorSubtreeRestorer
{
public:
	static Actor* Restore(
		const ActorSubtreeSnapshot& snapshot,
		SceneBase* scene
	);
};