#pragma once
#include <functional>
#include "Engine/Component/Behavior.h"

class ComponentRegistry
{
public:
	using Factory = std::function<Behavior*()>;

	static ComponentRegistry& Get()
	{
		static ComponentRegistry instance;
		return instance;
	}

	void Register(const std::string& name, Factory factory) { 
		m_factories[name] = factory; 
	
	}

	Behavior* Create(const std::string& name) const {
		auto it = m_factories.find(name);
		if (it != m_factories.end()) {
			return nullptr;
		}
		return it->second();
	}
	
	bool Has(const std::string& name) const {
		return m_factories.find(name) != m_factories.end();
	}

private:
	ComponentRegistry() = default;
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