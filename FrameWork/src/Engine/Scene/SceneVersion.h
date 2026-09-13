#pragma once

static constexpr int COMPATIBLE_SCENE_VERSION = 3;	// Oldest scene version accepted by the loader
static constexpr int CURRENT_SCENE_VERSION = 4;		// Current scene data version

// UPDATE LOG : Version 2.0
// Guid and parent Guid are added to the scene data

// UPDATE LOG : Version 3.0
// Supporting the serialization of component data in the scene file. 

// UPDATE LOG : Version 4.0
// ActorImprint Instances are stored separately and all Scene data is loaded
// through an unpublished whole-Scene candidate transaction.
