#pragma once
#include "Engine/Scene/SceneBase.h"
#include "Core/EditorSelection.h"

class HierarchyPanel
{
public:

	struct Callbacks
	{
		std::function<bool(const Guid& actorGuid, const std::string& newName)> onRenameActor;	// Callback for when an actor is renamed
		std::function<bool(const std::string& name, const Guid& parentGuid)> onCreateActor;		// Callback for when an actor is created
		std::function<bool(const Guid& actorGuid)> onDeleteActor;								// Callback for when an actor is deleted
		std::function<bool(const Guid& actorGuid, const Guid& newParentGuid)> onReparentActor;	// Callback for when an actor is reparented
		std::function<bool(const Guid& assetGuid, const Guid& parentGuid)> onInstantiateActorImprint;
		std::function<void(const Guid& actorGuid)> onOpenCanvas;								// Callback for opening a Canvas as an edit scope
		std::function<void()> onSelectionChanging;
		bool canEdit = true;	// Flag to indicate if the hierarchy panel is editable (e.g., in edit mode)
	};

	void Render(SceneBase* scene, EditorSelection& selection, const Callbacks& callbacks);
	static Guid ResolveEmptySpaceCreationParent(const SceneBase* scene);
	void SetDiagnostic(std::string diagnostic) { m_diagnostic = std::move(diagnostic); }
	const std::string& GetDiagnostic() const { return m_diagnostic; }

private:
	Guid m_actorToDeleteGuid;	// GUID of the actor that is pending deletion

	Guid m_creationParentGuid;	// GUID of the parent actor for the new actor being created (if any)

	bool m_showMenuPopup = false;	// Flag to indicate if the "Create Actor" context menu should be shown

	bool m_showActorCreationPopup = false;			// Flag to indicate if the "Create Actor" popup should be shown
	char m_newActorNameBuffer[128] = "NewActor";	// Buffer to hold the name of the new actor being created

	// Params for rename an actor
	Guid m_renameTargetGuid;
	bool m_showRenamePopup = false;
	char m_renameBuffer[128]{};
	std::string m_diagnostic;

private:
	// Drag-and-drop payload type for actors in the hierarchy panel
	static constexpr const char* kActorPayloadType = "101ENGINE_HIERARCHY_ACTOR";
private:
	void RenderActorNode(
		Actor* actor,
		SceneBase* scene,
		EditorSelection& selection,
		const Callbacks& callbacks
	);

	void RenderRootDropTarget(SceneBase* scene, EditorSelection& selection,
		const Callbacks& callbacks
	);

	void HandleActorDragSource(Actor* actor, const Callbacks& callbacks);

	void HandleDropTarget(
		SceneBase* scene,
		const Guid& newParentGuid,
		const Callbacks& callbacks
	);
	void ChangeSelection(EditorSelection& selection, const Guid& actorGuid,
		const Callbacks& callbacks);
};
