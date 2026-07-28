#pragma once
#include "Engine/Scene/SceneBase.h"

class HierarchyPanel
{
public:

	struct Callbacks
	{
		std::function<void(const std::string& name)> onCreateActor;	// Callback for when an actor is created
		std::function<void(Actor* actor)> onDeleteActor;			// Callback for when an actor is deleted
	};

    void Render(SceneBase* scene, const Callbacks& callbacks);
    Actor* GetSelectedActor() const { return m_pSelectedActor; }

	// Clears the current selection.
	// Must be called when the selected actor is destroyed (e.g. hot-reload is performed).
    void ClearSelection() { m_pSelectedActor = nullptr; }

private:
    void RenderActorNode(Actor* actor);

	Actor* m_pSelectedActor = nullptr;	// Currently selected actor in the hierarchy
	Actor* m_pActorToDelete = nullptr;	// Actor that is pending deletion (used for confirmation popup)

	bool m_showMenuPopup = false;	// Flag to indicate if the "Create Actor" context menu should be shown

	bool m_showActorCreationPopup = false;			// Flag to indicate if the "Create Actor" popup should be shown
	char m_newActorNameBuffer[128] = "NewActor";	// Buffer to hold the name of the new actor being created

};
