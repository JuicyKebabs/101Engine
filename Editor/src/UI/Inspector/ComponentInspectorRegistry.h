#pragma once
#include <functional>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include "Engine/Component/Component.h"
#include "UI/Inspector/InspectorContext.h"

//------------------------------------------------------------------------------------
// ComponentInspectorRegistry class
// This class holds a draw function for each component type.
// Registerd draw functions are used to draw the inspector UI for each component type.
//------------------------------------------------------------------------------------

class ComponentInspectorRegistry
{
public:
	// Alias for the drawer function type
	using Drawer = std::function<void(Component&, const InspectorContext&)>;

	// Template function to register a drawer for a specific component type
	template<class T>
	using TypeDrawer = std::function<void(T&, const InspectorContext&)>;

	// Register function
	template<class T>
	void Register(TypeDrawer<T> drawer)
	{
		// Ensure that T is derived from Component
		static_assert(
			std::is_base_of_v<Component, T>,
			"ComponentInspectorRegistry::Register<T>: "
			"T must derive from Component"
			);

		// Get the type index for the component type T
		const std::type_index typeId = std::type_index(typeid(T));

		// Store the drawer function in the registry
		m_drawers.insert_or_assign(
			typeId,
			[drawer = std::move(drawer)](Component& component, const InspectorContext& context)
			{
				// Cast the component to the specific type T
				drawer(static_cast<T&>(component), context);
			}
		);
	}

	bool Draw(Component& component, const InspectorContext& context) const;
	bool HasDrawer(const Component& component) const;

private:
	// Registry of drawer functions for each component type
	std::unordered_map<std::type_index, Drawer> m_drawers;
};