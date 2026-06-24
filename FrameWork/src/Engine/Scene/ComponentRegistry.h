#pragma once
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <typeindex>
#include "Engine/Component/Component.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Actor/Actor.h"

//-----------------------------------------------------------------------------------------------------------------------------------------
// ComponentRegistry class and registration system
// This registry allows the engine to create instances of all components (including user-defined ones) by their class name at runtime.
//------------------------------------------------------------------------------------------------------------------------------------------

// User-defined component and its factory function are stored in the registry by mapping.
// The registration is done by the helper macro REGISTER_COMPONENT, which should be placed in the component class header file.
// That header file has to be included in a .cpp file to make sure the registration macro is called at the global scope

class ComponentRegistry
{
public:
	using Factory = std::function<Component* ()>;	// Factory function type that creates a Component instance

	static ComponentRegistry& Get();

	// Register a behavior factory with a name and its type index
	void Register(const std::string& name, Factory factory, std::type_index typeId) 
	{ 
		m_factories[name] = factory;
		DBG("ComponentRegistry: REGISTER name='%s' typeid.name()='%s'", name.c_str(), typeId.name());
		m_typeNames[typeId.name()] = name;
	}

	// Specialized registration function for components defined in GameCode.dll
	void RegisterGameComponent(const std::string& name, Factory factory, std::type_index typeId)
	{
		Register(name, factory, typeId);
		m_gameComponentNames.insert(name);
		DBG("ComponentRegistry: REGISTERED GameCode component '%s'", name.c_str());
	}

	// Unregister all components that were registered from GameCode.dll (used for hot-reloading)
	void UnregisterAllGameComponents()
	{
		for (const auto& name : m_gameComponentNames)
		{
			m_factories.erase(name);

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
		auto it = m_factories.find(name);
		if (it != m_factories.end()) 
		{
			return it->second();
		}
		return nullptr;
	}
	
	// Check if a behavior factory exists in the registry
	bool Has(const std::string& name) const 
	{
		return m_factories.find(name) != m_factories.end();
	}

	// Create a component instance by name and add it to the given actor
	bool AddToActor(const std::string& name, Actor* actor) const
	{
		auto it = m_factories.find(name);
		if(it == m_factories.end())
		{
			DBG("ComponentRegistry: No factory found for component '%s'", name.c_str());
			return false;
		}

		std::unique_ptr<Component> component(it->second());

		if (component)
		{
			actor->AddComponent(std::move(component));
			return true;
		}

		DBG("ComponentRegistry: Factory for component '%s' failed to create an instance", name.c_str());
		return false;
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

private:
	ComponentRegistry() = default;

	// Map of component names to their factory functions
	std::unordered_map<std::string, Factory> m_factories;

	// Map of component type indices to their registered names
	std::unordered_map<std::string, std::string> m_typeNames;

	// Set of component names registered from GameCode.dll (used for hot-reloading)
	std::unordered_set <std::string> m_gameComponentNames;
};


// Helper macro to register a component class
// Usage: Place REGISTER_COMPONENT(YourComponentClass) in the .h file of your component class
// .cpp file must include the .h file to ensure the registration happens at global scope
#define REGISTER_COMPONENT(ClassName)                                   \
    static bool _reg_##ClassName = [](){                                \
        ComponentRegistry::Get().Register(                              \
            #ClassName,                                                 \
            [](){ return static_cast<Component*>(new ClassName()); },   \
            std::type_index(typeid(ClassName))                          \
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