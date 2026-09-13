#pragma once

#include <functional>
#include <span>

#include "Document/EditorDocumentManager.h"

class DocumentTabBar
{
public:
	struct Callbacks
	{
		std::function<void(EditorDocumentId)> onActivate;
		std::function<void(EditorDocumentId)> onClose;
		bool canInteract = true;
	};

	void Render(std::span<const EditorDocumentInfo> documents,
		const Callbacks& callbacks);

	static bool DispatchActivate(const Callbacks& callbacks, EditorDocumentId id)
	{
		if (!callbacks.canInteract || !callbacks.onActivate) return false;
		callbacks.onActivate(id);
		return true;
	}

	static bool DispatchClose(const Callbacks& callbacks, EditorDocumentId id)
	{
		if (!callbacks.canInteract || !callbacks.onClose) return false;
		callbacks.onClose(id);
		return true;
	}

private:
	// ImGui applies SetSelected on a later layout pass. Remember the tab that
	// was actually visible so model-driven selection can be synchronized
	// without mistaking the previously visible tab for a user selection.
	EditorDocumentId m_visibleDocumentId;
};
