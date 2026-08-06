#pragma once
#include <cstddef>
#include <functional>
#include <string>

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
    struct Callbacks
    {
		// Callback for adding a component to an actor
        std::function<bool(
            const Guid& actorGuid,
            const std::string& componentName
            )> onAddComponent;

		// Callback for removing a component from an actor
        std::function<bool(
            const Guid& actorGuid,
            const std::string& componentName,
            std::size_t occurrenceIndex
            )> onRemoveComponent;
    };

public:
    void Render(
        Actor* selectedActor,
		const InspectorContext& context,
		const Callbacks& callbacks
    );

	ComponentInspectorRegistry& GetComponentInspectorRegistry() { return m_componentInspectorRegistry; }

private:
	// Structure to hold requests for component removal
	// to avoid immediate removal during the rendering loop
    struct ComponentRemovalRequest
    {
        std::string componentName;
        std::size_t occurrenceIndex = 0;
        bool requested = false;
    };

	// Registry for component drawer functions
    ComponentInspectorRegistry m_componentInspectorRegistry;

private:
	// Helper function to draw the inspector UI for a single component
	// Returns if the "Remove" button was clicked for this component
    bool DrawComponent(
        Component& component,
        const InspectorContext& context
    );
};
