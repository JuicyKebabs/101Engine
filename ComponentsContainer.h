#pragma once
#include <vector>
#include <memory>
#include "Component.h"

// ComponentsContainer Class
class ComponentsContainer
{
public:
	// Add a component of type T to the container
	template<class T, class... Args>
	T* AddComponent(Args&&... args)
	{
		static_assert(std::is_base_of_v<Component, T>, "AddComponent<T>: T must derive from Component");
		auto c = std::make_unique<T>(std::forward<Args>(args)...);
		T* ptr = c.get();
		m_components.push_back(std::move(c));
		return ptr;
	}

	// Remove a component from the container
	void RemoveComponent(std::shared_ptr<Component> component) {
		m_components.erase(std::remove(m_components.begin(), m_components.end(), component), m_components.end());
	}

	// Get a component of type T from the container
	template<class T, class... Args>
	T* GetComponent(Args&&... args)
	{
		static_assert(std::is_base_of_v<Component, T>, "GetComponent<T>: T must derive from Component");
		for (const auto& component : m_components) {
			if (auto casted = std::dynamic_pointer_cast<T>(component)) {
				return casted.get();
			}
		}
		return nullptr; // Return nullptr if no component of type T is found
	}

private:
	std::vector<std::shared_ptr<Component>> m_components; // Container for components
};