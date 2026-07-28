#pragma once

class UIRenderer;
struct InspectorContext;

//-------------------------------------------------------------------------------------
// UIRendererInspector class
// This class is responsible for rendering the inspector UI for UIRenderer component
//-------------------------------------------------------------------------------------

class UIRendererInspector
{
public:
	static void Draw(UIRenderer& uiRenderer, const InspectorContext& context);
};
