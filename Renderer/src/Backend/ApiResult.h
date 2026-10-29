#pragma once

#include "Renderer/Result.h"

#include <Windows.h>

//----------------------------------------------------------------------------------
// ApiResult.h
// Defines a utility function to create a Result object representing an API failure
//----------------------------------------------------------------------------------

namespace rendering::backend
{

[[nodiscard]]
Result MakeApiFailure(const char* operation, HRESULT hr);

}