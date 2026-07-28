#include "UI/Inspector/ComponentInspectorRegistry.h"

bool ComponentInspectorRegistry::Draw(Component& component, const InspectorContext& context) const
{
	const std::type_index typeId = std::type_index(typeid(component));

	auto it = m_drawers.find(typeId);

	if (it == m_drawers.end()) return false;

	it->second(component, context);

	return true;
}

bool ComponentInspectorRegistry::HasDrawer(const Component& component) const
{
	const std::type_index typeId = std::type_index(typeid(component));
	return m_drawers.find(typeId) != m_drawers.end();
}