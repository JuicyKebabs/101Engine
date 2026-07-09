#include "GuidGenerator.h"

Guid GuidGenerator::Generate()
{
	Guid g;
	CoCreateGuid(&g.value); // Generate a new GUID using the Windows API
	return g;
}