#pragma once
#include <string>

//---------------------------------------
// Editor apearances configuration
// This is applied at startup
// Runtime changes are not supported yet
//---------------------------------------

struct ImGuiIO;

struct EditorFontConfig
{
	std::string filePath;		// Path to the font file (TTF, OTF, TTC)
	float sizePixels = 16.0f;	// Font size in pixels

	// Index inside a TTC font collection
	// Ignored by normal TTF/OTF fonts
	unsigned int fontIndex = 0;
};

namespace EditorTheme
{
	namespace Metrics
	{
		inline constexpr float PropertyLabelWidth = 120.0f;	// Width of the property label in the inspector panel
	}

	void ApplyStyle();

	bool LoadFont(
		ImGuiIO& io,
		const EditorFontConfig& config
	);
}