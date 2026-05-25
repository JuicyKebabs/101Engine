#pragma once
#include <functional>
#include "Engine/Component/Behavior.h"
#include "Engine/Core/Debug/Debug.h"

//------------------------------------------------------------------------------------------------------------------------
// ComponentRegistry class and registration system
// This registry allows the engine to create instances of user-defined behavior components by name when loading a scene.
//-------------------------------------------------------------------------------------------------------------------------

// User-defined component and it's factory funciotn is stored in the registry by mapping.
// The registration is done by the helper macro REGISTER_BEHAVIOR, which should be placed in the behavior class header file.
// That header file has to be included in a .cpp file to make sure the registration macro is called at the global scope

class ComponentRegistry
{
public:
	using Factory = std::function<Behavior*()>;

	static ComponentRegistry& Get()
	{
		static ComponentRegistry instance;
		return instance;
	}

	// Register a behavior factory with a name
	void Register(const std::string& name, Factory factory) 
	{ 
		m_factories[name] = factory; 
	}

	// Create a behavior instance by name from the registry
	Behavior* Create(const std::string& name) const 
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

private:
	ComponentRegistry() = default;

	// Map of behavior names to their factory functions(new instances function)
	std::unordered_map<std::string, Factory> m_factories;
};


// Helper macro to register a behavior class
// Usage: Place REGISTER_BEHAVIOR(YourBehaviorClass) in the .cpp file of your behavior class
#define REGISTER_BEHAVIOR(ClassName)					\
	static bool _reg_##ClassName = []{					\
		ComponentRegistry::Get().Register(#ClassName,	\
			[]() -> Behavior* { return new ClassName();	\
			});											\
		return true;									\
	}();