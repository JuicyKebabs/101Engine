#pragma once
#include "Engine/Component/Component.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
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
// Built-in factories and metadata are registered together in EngineComponentRegistration.cpp through RegisterReflected.
// GameCode registers through RegisterGameComponent and releases its entries before the module is unloaded.
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
		Factory factory;						// Factory function to create a Component instance
		std::type_index typeId;					// Type index of the Component type
		std::unique_ptr<TypeMetadata> metadata;	// Metadata for the Component type

		Entry(
			Factory componentFactory,
			std::type_index componentTypeId,
			std::unique_ptr<TypeMetadata> componentMetadata = {}
		)
		  : factory(std::move(componentFactory)),
			typeId(componentTypeId),
			metadata(std::move(componentMetadata))
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
		ComponentFamily family,
		std::unique_ptr<TypeMetadata> metadata = {}
	) 
	{ 
		m_entries.insert_or_assign(name, Entry(std::move(factory), typeId, std::move(metadata)));
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
	void RegisterGameComponent(
		const std::string& name,
		Factory factory,
		std::type_index typeId,
		std::unique_ptr<TypeMetadata> metadata)
	{
		Register(name, factory, typeId, ComponentCardinality::Multiple, ComponentFamily::None, std::move(metadata));
		m_gameComponentNames.insert(name);
		DBG("ComponentRegistry: REGISTERED GameCode component '%s'", name.c_str());
	}

	// Use authored metadata when provided; components without properties need no metadata builder.
	template<class T>
	bool RegisterGameComponent(const std::string& name)
	{
		auto metadata = [&]
		{
			if constexpr (requires { T::BuildMetadata(); }) return T::BuildMetadata();
			else return TypeMetadataBuilder<T>(name).Build();
		}();
		if (!metadata || metadata->GetType() != typeid(T) || metadata->GetStableTypeName() != name) return false;
		RegisterGameComponent(name, [] { return static_cast<Component*>(new T()); }, typeid(T),
			std::make_unique<TypeMetadata>(std::move(*metadata)));
		return true;
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

	// Register a reflected factory and its metadata together, before publishing either.
	template<class T>
	bool RegisterReflected(const std::string& name, std::unique_ptr<TypeMetadata> metadata)
	{
		if (!metadata || metadata->GetType() != typeid(T) || metadata->GetStableTypeName() != name) return false;
		using Policy = ComponentPolicy<T>;
		Register(name, [] { return static_cast<Component*>(new T()); }, typeid(T),
			Policy::cardinality, Policy::family, std::move(metadata));
		return true;
	}

	const TypeMetadata* GetMetadata(const std::string& name) const
	{
		auto entry = m_entries.find(name);
		return entry != m_entries.end() ? entry->second.metadata.get() : nullptr;
	}

	const TypeMetadata* GetMetadata(std::type_index typeId) const
	{
		const std::string name = GetNameByTypeIndex(typeId);
		return name.empty() ? nullptr : GetMetadata(name);
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


// Place once in the component's .cpp file. Uses ClassName::BuildMetadata() when provided.
#define REGISTER_GAME_COMPONENT(ClassName) \
	static const bool registered##ClassName = ComponentRegistry::Get().RegisterGameComponent<ClassName>(#ClassName);
