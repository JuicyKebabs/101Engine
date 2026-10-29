#pragma once

#include "TextureDesc.h"
#include "ResourceHandle.h"
#include "Result.h"

//--------------------------------------------------------------------------
// TextureRegistrationResult
// Registration result for a texture resource
// This structure is publicly exposed to the user of the rendering library.
//--------------------------------------------------------------------------

namespace rendering
{

struct [[nodiscard]] TextureRegistrationResult
{
	Result status;
	TextureHandle handle;
};

}