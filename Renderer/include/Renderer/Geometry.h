#pragma once

#include "GeometryDesc.h"
#include "ResourceHandle.h"
#include "Result.h"

//--------------------------------------------------------------------------
// GeometryRegistrationResult
// Registration result for a geometry resource
// This structure is publicly exposed to the user of the rendering library.
//--------------------------------------------------------------------------

namespace rendering
{

struct [[nodiscard]] GeometryRegistrationResult
{
	Result status;
	GeometryHandle handle;
};

}