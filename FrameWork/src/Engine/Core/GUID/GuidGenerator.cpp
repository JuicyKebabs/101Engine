#include "GuidGenerator.h"

Guid GuidGenarator::Generat()
{
	Guid g;
	CoCreateGuid(&g.value); // Generate a new GUID using the Windows API
	return g;
}