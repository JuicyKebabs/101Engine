#pragma once

class Collider;
struct InspectorContext;

//-------------------------------------------------------------------------------------
// ColliderInspector class
// This class is responsible for rendering the inspector UI for Collider component
//-------------------------------------------------------------------------------------

class ColliderInspector
{
public:
	static void Draw(Collider& collider, const InspectorContext& context);
};
