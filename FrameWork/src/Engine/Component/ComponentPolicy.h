#pragma once
#include <tuple>

class Transform;
class RectTransform;
class Behaviour;
class Camera;
class MeshRenderer;
class SpriteRenderer;
class UIRenderer;
class UIImage;

//-------------------------------------------------------------------------------------
// ComponentPolicy
// Every component type has a policy which defines how it can be used in an actor.
// Cardinarity : how many instances of this component type can be attached to an actor
// Family : a grouping of component types that are related
//-------------------------------------------------------------------------------------


enum class ComponentCardinality
{
	UniqueRequired,	// Only one instance allowed and it must be present
	UniqueOptional,	// Only one instance allowed, but it can be absent
	Multiple		// Multiple instances allowed
};

enum class ComponentFamily
{
	None,
	Transform,
	Renderer
};

// Runtime representation of a registered component's policy.
// ComponentPolicy<T> defines the policy at compile time, while this structure
// allows ComponentRegistry to expose only the policy required by callers.
struct ComponentPolicyInfo
{
	ComponentCardinality cardinality;
	ComponentFamily family;
};

template<class Component>
struct ComponentPolicy
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::UniqueOptional;
	static constexpr ComponentFamily family = ComponentFamily::None;
};

template <>
struct ComponentPolicy<Transform>
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::UniqueRequired;
	static constexpr ComponentFamily family = ComponentFamily::Transform;
};

template <>
struct ComponentPolicy<RectTransform>
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::UniqueRequired;
	static constexpr ComponentFamily family = ComponentFamily::Transform;
};

template<>
struct ComponentPolicy<Behaviour>
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::Multiple;
	static constexpr ComponentFamily family = ComponentFamily::None;
};

template<>
struct ComponentPolicy<Camera>
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::UniqueOptional;
	static constexpr ComponentFamily family = ComponentFamily::None;
};

template<>
struct ComponentPolicy<MeshRenderer>
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::UniqueOptional;
	static constexpr ComponentFamily family = ComponentFamily::Renderer;
};

template<>
struct ComponentPolicy<SpriteRenderer>
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::UniqueOptional;
	static constexpr ComponentFamily family = ComponentFamily::Renderer;
};

template<>
struct ComponentPolicy<UIRenderer>
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::UniqueOptional;
	static constexpr ComponentFamily family = ComponentFamily::Renderer;
};

template<>
struct ComponentPolicy<UIImage>
{
	static constexpr ComponentCardinality cardinality = ComponentCardinality::UniqueOptional;
	static constexpr ComponentFamily family = ComponentFamily::Renderer;
};
