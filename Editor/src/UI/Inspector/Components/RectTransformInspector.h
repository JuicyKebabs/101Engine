#pragma once

class RectTransform;
struct InspectorContext;

//-------------------------------------------------------------------------------------
// RectTransformInspector class
// This class is responsible for rendering the inspector UI for RectTransform component
//-------------------------------------------------------------------------------------

class RectTransformInspector
{
public:
	static void Draw(RectTransform& rectTransform, const InspectorContext& context);
};
