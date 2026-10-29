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

// GeometryData is a value copied from one accepted Geometry Content revision.
// The handle identifies the registration; revision identifies its current contents.
// CPU information is available before upload and never waits for GPU completion.
struct GeometryData
{
    Vector3 boundsCenter;
    float boundsRadius = 0;
    std::uint64_t revision = 0;
};

struct [[nodiscard]] GeometryRegistrationResult
{
	Result status;
	GeometryHandle handle;
};

}