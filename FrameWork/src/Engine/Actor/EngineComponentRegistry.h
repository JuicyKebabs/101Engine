#pragma once
#pragma once
#include <string>
#include <unordered_map>
#include <functional>

//---------------------------------------------------------------------------------
// Built-in component registry system
// This allows components to be atached to actors at startup and created by name (string).
//---------------------------------------------------------------------------------

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

	void Register(const std::string& name, Adder adder)
	{
		m_adders[name] = adder;
	}

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
	std::unordered_map<std::string, Adder> m_adders;	// Component registry mapping component names to adder functions
};