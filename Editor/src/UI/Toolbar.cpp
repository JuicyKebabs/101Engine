#include "Toolbar.h"
#include "UI/EditorUI.h"
#include "imgui.h"

void Toolbar::Render(const Callbacks& callbacks)
{
	const ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::Begin("Toolbar", nullptr, flags);

	{
		EditorUI::DisabledScope disabledScope(!callbacks.canPlay);

		if (ImGui::Button("Play") && callbacks.onPlay)
		{
			callbacks.onPlay();
		}
	}

	ImGui::SameLine();

	{
		EditorUI::DisabledScope disabledScope(!callbacks.canStop);

		if (ImGui::Button("Stop") && callbacks.onStop)
		{
			callbacks.onStop();
		}
	}

	ImGui::SameLine();

	if (callbacks.canStop)
	{
		ImGui::TextColored(ImVec4(0.35f, 0.80f, 0.45f, 1.0f), "Playing");
	}
	else
	{
		ImGui::TextColored(ImVec4(0.35f, 0.45f, 0.80f, 1.0f), "Edit Mode");
	}

	ImGui::End();
}