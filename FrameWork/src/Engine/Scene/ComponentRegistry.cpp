#include "ComponentRegistry.h"

ComponentRegistry& ComponentRegistry::Get()
{
	static ComponentRegistry instance;
	return instance;
}