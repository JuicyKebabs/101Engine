// R7 implementation guide: section 4.6.
// Small internal adapter to the existing Result contract.
// Ordinary failures do not imply DeviceLost or unknown GPU completion.
// The caller that observes a GPU failure is responsible for preserving its fatal classification.

#pragma once

#include "Renderer/Result.h"

#include <utility>

namespace rendering::detail
{

inline Result Fail(std::string message)
{
	Result failure;
	failure.succeeded = false;
	failure.message = std::move(message);
	return failure;
}

}
