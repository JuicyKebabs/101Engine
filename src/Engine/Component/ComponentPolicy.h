#pragma once
#include <tuple>

class Transform;
class Behaviour;
class Camera;
class MeshRenderer;

enum class ComponentCardinality
{
	UniqueRequired,	// Only one instance allowed and it must be present
	UniqueOptional,	// Only one instance allowed, but it can be absent
	Multiple		// Multiple instances allowed
};

template<class Component>
struct ComponentPolicy
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::UniqueOptional;
};

template <>
struct ComponentPolicy<Transform>
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::UniqueRequired;
};

template<>
struct ComponentPolicy<Behaviour>
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::Multiple;
};

template<>
struct ComponentPolicy<Camera>
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::UniqueOptional;
};

template<>
struct ComponentPolicy<MeshRenderer>
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::UniqueOptional;
};