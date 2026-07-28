#pragma once

class Canvas;
struct InspectorContext;

//-------------------------------------------------------------------------------------
// CanvasInspector class
// This class is responsible for rendering the inspector UI for Canvas component
//-------------------------------------------------------------------------------------

class CanvasInspector
{
public:
	static void Draw(Canvas& canvas, const InspectorContext& context);
};
