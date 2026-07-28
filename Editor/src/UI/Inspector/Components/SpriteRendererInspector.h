#pragma once

class SpriteRenderer;
struct InspectorContext;

//-------------------------------------------------------------------------------------
// SpriteRendererInspector class
// This class is responsible for rendering the inspector UI for SpriteRenderer component
//-------------------------------------------------------------------------------------

class SpriteRendererInspector
{
public:
	static void Draw(SpriteRenderer& spriteRenderer, const InspectorContext& context);
};
