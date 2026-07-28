#pragma once
#include "Engine/Actor/Actor.h"
#include "UI/Inspector/ComponentInspectorRegistry.h"
#include "UI/Inspector/InspectorContext.h"

//-----------------------------------------------------------------
// InspectorPanel class
// Draws the inspector panel for the selected actor and components.
//-----------------------------------------------------------------

class InspectorPanel
{
public:
    void Render(
        Actor* selectedActor,
		const InspectorContext& context
    );

	ComponentInspectorRegistry& GetComponentInspectorRegistry() { return m_componentInspectorRegistry; }

private:
	// Registry for component drawer functions
    ComponentInspectorRegistry m_componentInspectorRegistry;

	// Buffer for component name input field
    char m_componentNameBuffer[128] = "";

private:
	// Helper function to draw the inspector UI for a single component
    void DrawComponent(
        Component& component,
        const InspectorContext& context
    );
};
