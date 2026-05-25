#pragma once
#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include "Engine/Core/Debug/Debug.h"

//-----------------------------------------------------------------------------------------
// Built-in component registry system
// This allows components to be atached to actors at startup and created by name (string).
//-----------------------------------------------------------------------------------------

class Actor;

class EngineComponentRegistry
{
public:
	using Adder = std::function<void(Actor*)>;

	static EngineComponentRegistry& Get()
	{
		static EngineComponentRegistry instance;
		return instance;
	}

	// Register a component adder function with a name
	void Register(const std::string& name, Adder adder)
	{
		m_adders[name] = adder;
		DBG("Registered engine component: %s", name.c_str());
	}

	// Add component to the actor by name from the registry
	bool Add(const std::string& name, Actor* actor) const
	{
		auto it = m_adders.find(name);
		if (it != m_adders.end())
		{
			it->second(actor); // Call the adder function to add the component to the actor
			return true;
		}
		return false; // Component name not found in the registry
	}

	bool Has(const std::string& name) const
	{
		return m_adders.find(name) != m_adders.end();
	}

private:
	EngineComponentRegistry() = default;

	// Component registry mapping component names to adder functions
	std::unordered_map<std::string, Adder> m_adders;
};

// Helper macro to register an engine component class
#define REGISTER_ENGINE_COMPONENT(ClassName)                          \
    static bool _reg_engine_##ClassName = [](){                       \
        EngineComponentRegistry::Get().Register(#ClassName,           \
            [](Actor* a){ a->AddComponent<ClassName>(); });           \
        return true;                                                   \
    }();