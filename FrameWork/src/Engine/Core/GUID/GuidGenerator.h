#pragma once
#include "Guid.h"

//-----------------------------------------------------------------------------------------------------
// GuidGenarator class
// This class is  responsible for generating globally unique identifiers (GUIDs) using the Windows API.
//-----------------------------------------------------------------------------------------------------

class GuidGenerator
{
public:
	static Guid Generate();
};