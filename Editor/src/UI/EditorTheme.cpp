#include "UI/EditorTheme.h"
#include <filesystem>
#include "Engine/Core/Debug/Debug.h"
#include "imgui.h"

namespace
{
	ImVec4 Color(float r, float g, float b, float a)
	{
		return ImVec4(r, g, b, a);
	}

	ImFont* AddFallbackFont(
		ImGuiIO& io,
		float sizePixels
	)
	{
		ImFontConfig config;
		config.SizePixels = sizePixels;

		return io.Fonts->AddFontDefault(&config);
	}
}

void EditorTheme::ApplyStyle()
{
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();

	// Leyout
	style.WindowPadding = ImVec2(8.0f, 8.0f);
	style.FramePadding = ImVec2(6.0f, 4.0f);
	style.CellPadding = ImVec2(6.0f, 4.0f);
	style.ItemSpacing = ImVec2(8.0f, 5.0f);
	style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
	style.IndentSpacing = 18.0f;
	style.ScrollbarSize = 13.0f;
	style.GrabMinSize = 10.0f;

	// Shape
	style.WindowRounding = 4.0f;
	style.ChildRounding = 3.0f;
	style.FrameRounding = 3.0f;
	style.PopupRounding = 4.0f;
	style.ScrollbarRounding = 6.0f;
	style.GrabRounding = 3.0f;
	style.TabRounding = 3.0f;

	// Borders
	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.TabBorderSize = 0.0f;

	ImVec4* colors = style.Colors;

	// Neutral gray foundation
	colors[ImGuiCol_Text] = Color(0.88f, 0.89f, 0.91f, 1.0f);
	colors[ImGuiCol_TextDisabled] = Color(0.49f, 0.51f, 0.55f, 1.0f);
	colors[ImGuiCol_WindowBg] = Color(0.095f, 0.10f, 0.11f, 1.0f);
	colors[ImGuiCol_ChildBg] = Color(0.095f, 0.10f, 0.11f, 1.0f);
	colors[ImGuiCol_PopupBg] = Color(0.12f, 0.125f, 0.14f, 0.98f);
	colors[ImGuiCol_Border] = Color(0.24f, 0.25f, 0.28f, 1.0f);
	colors[ImGuiCol_BorderShadow] = Color(0.0f, 0.0f, 0.0f, 0.0f);

	// Input fields
	colors[ImGuiCol_FrameBg] = Color(0.16f, 0.17f, 0.19f, 1.0f);
	colors[ImGuiCol_FrameBgHovered] = Color(0.21f, 0.22f, 0.25f, 1.0f);
	colors[ImGuiCol_FrameBgActive] = Color(0.24f, 0.27f, 0.31f, 1.0f);

	// Window and menu headers
	colors[ImGuiCol_TitleBg] = Color(0.075f, 0.08f, 0.09f, 1.0f);
	colors[ImGuiCol_TitleBgActive] = Color(0.13f, 0.14f, 0.16f, 1.0f);
	colors[ImGuiCol_TitleBgCollapsed] = Color(0.075f, 0.08f, 0.09f, 1.0f);
	colors[ImGuiCol_MenuBarBg] = Color(0.12f, 0.125f, 0.14f, 1.0f);

	// Scrollbars
	colors[ImGuiCol_ScrollbarBg] = Color(0.08f, 0.085f, 0.095f, 1.0f);
	colors[ImGuiCol_ScrollbarGrab] = Color(0.25f, 0.26f, 0.29f, 1.0f);
	colors[ImGuiCol_ScrollbarGrabHovered] = Color(0.34f, 0.35f, 0.39f, 1.0f);
	colors[ImGuiCol_ScrollbarGrabActive] = Color(0.40f, 0.42f, 0.46f, 1.0f);

	// Restrained blue-gray accent
	const ImVec4 accent = Color(0.34f, 0.52f, 0.70f, 1.0f);
	const ImVec4 accentHovered = Color(0.40f, 0.60f, 0.79f, 1.0f);
	const ImVec4 accentActive = Color(0.29f, 0.46f, 0.64f, 1.0f);

	colors[ImGuiCol_CheckMark] = accent;
	colors[ImGuiCol_CheckboxSelectedBg] = accentActive;
	colors[ImGuiCol_SliderGrab] = accent;
	colors[ImGuiCol_SliderGrabActive] = accentHovered;
	colors[ImGuiCol_InputTextCursor] = accentHovered;

	// Buttons
	colors[ImGuiCol_Button] = Color(0.19f, 0.20f, 0.23f, 1.0f);
	colors[ImGuiCol_ButtonHovered] = Color(0.26f, 0.31f, 0.36f, 1.0f);
	colors[ImGuiCol_ButtonActive] = Color(0.29f, 0.40f, 0.51f, 1.0f);

	// Headers, selectable items and tree nodes
	colors[ImGuiCol_Header] = Color(0.17f, 0.18f, 0.21f, 1.0f);
	colors[ImGuiCol_HeaderHovered] = Color(0.25f, 0.30f, 0.35f, 1.0f);
	colors[ImGuiCol_HeaderActive] = Color(0.28f, 0.39f, 0.50f, 1.0f);

	// Separators
	colors[ImGuiCol_Separator] = Color(0.24f, 0.25f, 0.28f, 1.0f);
	colors[ImGuiCol_SeparatorHovered] = accentHovered;
	colors[ImGuiCol_SeparatorActive] = accentActive;

	// Tabs
	colors[ImGuiCol_Tab] = Color(0.13f, 0.14f, 0.16f, 1.0f);
	colors[ImGuiCol_TabHovered] = Color(0.25f, 0.32f, 0.39f, 1.0f);
	colors[ImGuiCol_TabSelected] = Color(0.21f, 0.27f, 0.33f, 1.0f);
	colors[ImGuiCol_TabSelectedOverline] = accent;
	colors[ImGuiCol_TabDimmed] = Color(0.10f, 0.105f, 0.12f, 1.0f);
	colors[ImGuiCol_TabDimmedSelected] = Color(0.16f, 0.17f, 0.20f, 1.0f);

	// Tables and docking
	colors[ImGuiCol_TableHeaderBg] = Color(0.16f, 0.17f, 0.19f, 1.0f);
	colors[ImGuiCol_TableBorderStrong] = Color(0.25f, 0.26f, 0.29f, 1.0f);
	colors[ImGuiCol_TableBorderLight] = Color(0.18f, 0.19f, 0.21f, 1.0f);
	colors[ImGuiCol_DockingPreview] = Color(0.34f, 0.52f, 0.70f, 0.65f);
	colors[ImGuiCol_DockingEmptyBg] = Color(0.075f, 0.08f, 0.09f, 1.0f);
	colors[ImGuiCol_TextSelectedBg] = Color(0.34f, 0.52f, 0.70f, 0.35f);


	colors[ImGuiCol_DragDropTarget] = accentHovered;

	colors[ImGuiCol_NavCursor] = accentHovered;

	colors[ImGuiCol_ModalWindowDimBg] = Color(0.02f, 0.02f, 0.025f, 0.72f);
}

bool EditorTheme::LoadFont(
	ImGuiIO& io,
	const EditorFontConfig& config
)
{
	std::error_code pathError;

	const bool fontExists = std::filesystem::exists(config.filePath, pathError);

	const float fallbackSize = config.sizePixels > 0.0f ? config.sizePixels : 16.0f;

	// Fallback to default font if the specified font file is not found or invalid
	if (config.filePath.empty()		||
		config.sizePixels <= 0.0f	||
		pathError					||
		!fontExists)
	{
		DBG("EditorTheme: Font file was not found: '%s'. " "Using fallback font.", config.filePath.c_str());

		io.FontDefault = AddFallbackFont(io, fallbackSize);
		return false;
	}

	// Configure font settings
	ImFontConfig imguiFontConfig;
	imguiFontConfig.FontNo = config.fontIndex;

	// Load the specified font file
	ImFont* loadedFont = io.Fonts->AddFontFromFileTTF(
		config.filePath.c_str(), 
		config.sizePixels, 
		&imguiFontConfig
	);

	// Fallback to default font if the specified font file is not found or invalid
	if (!loadedFont)
	{
		DBG("EditorTheme: Failed to load font: '%s'. " "Using fallback font.", config.filePath.c_str());
		io.FontDefault = AddFallbackFont(io, fallbackSize);
		return false;
	}

	// Set the loaded font as the default font
	io.FontDefault = loadedFont;

	return true;
}