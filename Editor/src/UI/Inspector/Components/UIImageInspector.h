#pragma once

class UIImage;
struct InspectorContext;

//-------------------------------------------------------------------------------------
// UIImageInspector class
// This class is responsible for rendering the inspector UI for UIImage component
//-------------------------------------------------------------------------------------

class UIImageInspector
{
public:
	static void Draw(UIImage& image, const InspectorContext& context);
};
