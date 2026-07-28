#pragma once

//-------------------------------------------------------------------------------------
// MeshRendererInspector class
// This class is responsible for rendering the inspector UI for MeshRenderer component
//-------------------------------------------------------------------------------------

class MeshRenderer;
struct InspectorContext;

class MeshRendererInspector
{
public:
	static void Draw(MeshRenderer& meshRenderer, const InspectorContext& context);
};