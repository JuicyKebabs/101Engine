#pragma once
#include "Engine/Core/Reflection/ActorReferenceCodec.h"

//--------------------------------------------------------------------------------------------------------------------
// SceneActorReferenceContext class
// This class provides context for validating and resolving ActorReferences within a Scene.
// Guid validation in the context of the scene and finding actors by their Guid within the scene are handled here.
// Absorbs the scene reference to provide the necessary context for ActorReference serialization and deserialization.
//--------------------------------------------------------------------------------------------------------------------

class SceneBase;

class SceneActorReferenceContext final :
	public ActorReferenceSaveContext,
	public ActorReferenceRestoreContext
{
public:
	explicit SceneActorReferenceContext(const SceneBase& scene)
		: m_scene(scene)
	{}

	ActorReferenceCodecResult Validate(const Guid& guid) const override;
	ActorReferenceCodecResult FindActor(const Guid& guid, Actor*& outActor) const override;

private:
	const SceneBase& m_scene;
};
