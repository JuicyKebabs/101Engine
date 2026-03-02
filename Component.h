#pragma once

// Forward declaration
class Actor;

// Component class 
class Component
{
public:
	Component(Actor* owner) : m_pOwner(owner) {}
	virtual ~Component() = default;

	Actor* GetOwner();

private:
	Actor* m_pOwner = nullptr;
};