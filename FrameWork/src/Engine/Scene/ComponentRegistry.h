#pragma once
#include "Engine/Component/Component.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Actor/Actor.h"
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <typeindex>
#include <algorithm>
#include <vector>
#include <optional>

//-----------------------------------------------------------------------------------------------------------------------------------------
// ComponentRegistry class and registration system
// This registry allows the engine to create instances of all components (including user-defined ones) by their class name at runtime.
// User-defined component and its factory function are stored in the registry by mapping.
// The registration is done by the helper macro REGISTER_COMPONENT, which should be placed in the component class header file.
// That header file has to be included in a .cpp file to make sure the registration macro is called at the global scope
//------------------------------------------------------------------------------------------------------------------------------------------

class ComponentRegistry
{
public:
	using Factory = std::function<Component* ()>;	// Factory function type that creates a Component instance

private:
	// Internal registration data. Callers should request only the factory result,
	// type ID, or policy they need through the corresponding public API.
	struct Entry
	{
		Factory factory;
		std::type_index typeId;

		// Constructor forn initialization
		Entry(
			Factory componentFactory,
			std::type_index componentTypeId
		)
		  : factory(std::move(componentFactory)),
			typeId(componentTypeId)
		{}
	};

public:

	static ComponentRegistry& Get();

	// Register a behavior factory with a name and its type index
	void Register(
		const std::string& name, 
		Factory factory, 
		std::type_index typeId,
		ComponentCardinality cardinality,
		ComponentFamily family
	) 
	{ 
		m_entries.insert_or_assign(name, Entry(std::move(factory), typeId));
		RegisterPolicy(typeId, { cardinality, family });
		DBG("ComponentRegistry: REGISTER name='%s' typeid.name()='%s'", name.c_str(), typeId.name());
		m_typeNames[typeId.name()] = name;
	}

	// Register policy metadata without exposing the component through a factory.
	// This is used by internal component types which can be attached in code but
	// must not appear in serialization or the Inspector's Add Component list.
	void RegisterPolicy(std::type_index typeId, ComponentPolicyInfo policy)
	{
		m_policies.insert_or_assign(typeId.name(), policy);
	}

	// Specialized registration function for components defined in GameCode.dll
	void RegisterGameComponent(const std::string& name, Factory factory, std::type_index typeId)
	{
		Register(name, factory, typeId, ComponentCardinality::Multiple, ComponentFamily::None);
		m_gameComponentNames.insert(name);
		DBG("ComponentRegistry: REGISTERED GameCode component '%s'", name.c_str());
	}

	// Unregister all components that were registered from GameCode.dll (used for hot-reloading)
	void UnregisterAllGameComponents()
	{
		for (const auto& name : m_gameComponentNames)
		{
			auto entryIt = m_entries.find(name);
			if (entryIt != m_entries.end())
			{
				m_policies.erase(entryIt->second.typeId.name());
			}

			m_entries.erase(name);

			// Delete from type names map as well
			for (auto it = m_typeNames.begin(); it != m_typeNames.end(); )
			{
				if (it->second == name)
				{
					it = m_typeNames.erase(it);
				}
				else
				{
					++it;
				}
			}

			DBG("ComponentRegistry: UNREGISTERED GameCode component '%s'", name.c_str());
		}

		m_gameComponentNames.clear();
	}

	// Create a component instance by name from the registry
	Component* Create(const std::string& name) const 
	{
		auto it = m_entries.find(name);
		if (it != m_entries.end()) 
		{
			return it->second.factory();
		}
		return nullptr;
	}
	
	// Check if a behavior factory exists in the registry
	bool Has(const std::string& name) const 
	{
		return m_entries.find(name) != m_entries.end();
	}

	// Create a component instance by name and add it to the given actor
	bool AddToActor(const std::string& name, Actor* actor) const
	{
		if (!actor) return false;

		auto it = m_entries.find(name);

		if(it == m_entries.end())
		{
			DBG("ComponentRegistry: No factory found for component '%s'", name.c_str());
			return false;
		}

		const Entry& entry = it->second;

		if (!actor->CanAddComponent(entry.typeId))
		{
			DBG("ComponentRegistry: Component '%s' cannot be added to Actor '%s'.", name.c_str(), actor->GetName().c_str());
			return false;
		}

		std::unique_ptr<Component> component(entry.factory());

		if (!component) return false;

		return actor->AddComponent(std::move(component)) != nullptr;
	}

	// Get name of a component by its type index
	std::string GetNameByTypeIndex(std::type_index typeId) const
	{
		auto it = m_typeNames.find(typeId.name());
		if (it == m_typeNames.end())
		{
			DBG("ComponentRegistry: No name found for component type index '%s'", typeId.name());
			return "";
		}
		return it->second;
	}

	// Get a list of all registered component names, sorted alphabetically
	// Used by inspector panel to display available components for addition to an actor
	std::vector<std::string> GetRegisteredComponentNames() const
	{
		std::vector<std::string> names;
		names.reserve(m_entries.size());

		for (const auto& [name, entry] : m_entries)
		{
			names.push_back(name);
		}

		std::sort(names.begin(), names.end());

		return names;
	}

	std::optional<std::type_index> GetTypeId(const std::string& name) const
	{
		auto it = m_entries.find(name);
		if (it == m_entries.end()) return std::nullopt;
		return it->second.typeId;
	}

	std::optional<ComponentPolicyInfo> GetPolicy(const std::string& name) const
	{
		auto it = m_entries.find(name);
		if (it == m_entries.end()) return std::nullopt;
		return GetPolicy(it->second.typeId);
	}

	std::optional<ComponentPolicyInfo> GetPolicy(std::type_index typeId) const
	{
		auto it = m_policies.find(typeId.name());
		if (it == m_policies.end()) return std::nullopt;
		return it->second;
	}

	// Check if a component can be added to the given actor 
	// based on its name and the actor's existing components
	// Used by inspector panel to determine if a component can be added to an actor
	bool CanAddToActor(const std::string& name, const Actor* actor) const
	{
		if (!actor) return false;

		const auto typeId = GetTypeId(name);
		if (!typeId) return false;

		return actor->CanAddComponent(*typeId);
	}

private:
	ComponentRegistry() = default;

	// Map of component names to their Entry struct containing factory and type information
	std::unordered_map<std::string, Entry> m_entries;

	// Map of component type indices to their registered names
	std::unordered_map<std::string, std::string> m_typeNames;

	// Runtime component policies are stored separately from factory entries so
	// internal component types can participate in Actor constraints without
	// becoming creatable or serializable by registered name.
	std::unordered_map<std::string, ComponentPolicyInfo> m_policies;

	// Set of component names registered from GameCode.dll (used for hot-reloading)
	std::unordered_set <std::string> m_gameComponentNames;
};


// Helper macro to register a component class
// Usage: Place REGISTER_COMPONENT(YourComponentClass) in the .h file of your component class
// .cpp file must include the .h file to ensure the registration happens at global scope
#define REGISTER_COMPONENT(ClassName)                                   \
    static bool _reg_##ClassName = [](){                                \
        using Policy = ComponentPolicy<ClassName>;                      \
        ComponentRegistry::Get().Register(                              \
            #ClassName,                                                 \
            [](){ return static_cast<Component*>(new ClassName()); },   \
            std::type_index(typeid(ClassName)),                         \
            Policy::cardinality,                                        \
            Policy::family                                              \
        );                                                              \
        return true;                                                    \
    }();

// Helper macro to register a component class defined in GameCode.dll (for hot-reloading support)
// Usage: Place REGISTER_GAME_COMPONENT(YourComponentClass) in the .h file of your component class defined in GameCode.dll
#define REGISTER_GAME_COMPONENT(ClassName)                              \
	static bool _reg_##ClassName = [](){                                \
		ComponentRegistry::Get().RegisterGameComponent(                 \
			#ClassName,                                                 \
			[](){ return static_cast<Component*>(new ClassName()); },   \
			std::type_index(typeid(ClassName))                          \
		);                                                              \
		return true;                                                    \
	}();
