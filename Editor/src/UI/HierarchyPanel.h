#pragma once
#include "Engine/Scene/SceneBase.h"

class HierarchyPanel
{
public:

	struct Callbacks
	{
		std::function<bool(const Guid& actorGuid, const std::string& newName)> onRenameActor;	// Callback for when an actor is renamed
		std::function<void(const std::string& name, const Guid& parentGuid)> onCreateActor;		// Callback for when an actor is created
		std::function<bool(const Guid& actorGuid)> onDeleteActor;								// Callback for when an actor is deleted
		std::function<bool(const Guid& actorGuid, const Guid& newParentGuid)> onReparentActor;	// Callback for when an actor is reparented
	};

    void Render(SceneBase* scene, const Callbacks& callbacks);
    Actor* GetSelectedActor(SceneBase* scene);

	// Clears the current selection.
	// Must be called when the selected actor is destroyed (e.g. hot-reload is performed).
    void ClearSelection() { m_selectedActorGuid = {}; }

private:
	Guid m_selectedActorGuid;	// GUID of the currently selected actor in the hierarchy
	Guid m_actorToDeleteGuid;	// GUID of the actor that is pending deletion

	Guid m_creationParentGuid;	// GUID of the parent actor for the new actor being created (if any)

	bool m_showMenuPopup = false;	// Flag to indicate if the "Create Actor" context menu should be shown

	bool m_showActorCreationPopup = false;			// Flag to indicate if the "Create Actor" popup should be shown
	char m_newActorNameBuffer[128] = "NewActor";	// Buffer to hold the name of the new actor being created

	// Params for rename an actor
	Guid m_renameTargetGuid;
	bool m_showRenamePopup = false;
	char m_renameBuffer[128]{};

private:
	// Drag-and-drop payload type for actors in the hierarchy panel
	static constexpr const char* kActorPayloadType = "101ENGINE_HIERARCHY_ACTOR";
private:
	void RenderActorNode(
		Actor* actor,
		SceneBase* scene,
		const Callbacks& callbacks
	);

	void RenderRootDropTarget(
		const Callbacks& callbacks
	);

	void HandleActorDragSource(Actor* actor);

	void HandleActorDropTarget(
		const Guid& newParentGuid,
		const Callbacks& callbacks
	);
};
