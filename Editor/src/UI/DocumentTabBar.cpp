#include "UI/DocumentTabBar.h"

#include <string>

#include "imgui.h"

void DocumentTabBar::Render(
	std::span<const EditorDocumentInfo> documents,
	const Callbacks& callbacks)
{
	EditorDocumentId activeDocumentId;

	for (const EditorDocumentInfo& document : documents)
	{
		if (!document.isActive)
		{
			continue;
		}

		activeDocumentId = document.id;
		break;
	}

	const bool isSynchronizingSelection = activeDocumentId.IsValid() &&
		activeDocumentId != m_visibleDocumentId;

	if (!ImGui::BeginTabBar("##EditorDocuments",
			ImGuiTabBarFlags_FittingPolicyScroll))
	{
		return;
	}

	EditorDocumentId visibleDocumentId;

	for (const EditorDocumentInfo& document : documents)
	{
		bool open = true;
		std::string label = document.displayName;

		if (document.isDirty)
		{
			label += " *";
		}

		label += "###EditorDocument" + std::to_string(document.id.value);
		const ImGuiTabItemFlags flags =
			isSynchronizingSelection && document.isActive ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;

		if (ImGui::BeginTabItem(label.c_str(), callbacks.canInteract ? &open : nullptr, flags))
		{
			visibleDocumentId = document.id;
			ImGui::EndTabItem();
		}

		if (!open)
		{
			DispatchClose(callbacks, document.id);
		}
	}

	ImGui::EndTabBar();

	if (!visibleDocumentId.IsValid())
	{
		if (documents.empty())
		{
			m_visibleDocumentId = {};
		}

		return;
	}

	if (isSynchronizingSelection)
	{
		if (visibleDocumentId == activeDocumentId)
		{
			m_visibleDocumentId = visibleDocumentId;
		}

		return;
	}

	m_visibleDocumentId = visibleDocumentId;

	if (visibleDocumentId != activeDocumentId)
	{
		DispatchActivate(callbacks, visibleDocumentId);
	}
}
