#pragma once
#include <string>

//----------------------------------------------------------------
// Result.h
// Defines a struct to represent the result of an operation
// indicating success or failure, along with an optional message.
//----------------------------------------------------------------

namespace rendering
{

enum class FailureKind
{
	General,				// General failure, Deal with it as a normal error.
	DeviceLost,				// Device lost or removed, Stop rendering and finalize the backend.
	GpuCompletionUnknown,	// GPU completion status is unknown, Finish the application without finalizing the backend.
};

// Struct to represent the result of an operation, 
// indicating success or failure and providing an optional message.
struct [[nodiscard("Check the result for safety")]] Result
{
	bool succeeded = true;
	std::string message;

	// Use only when the succeeded flag is false.
	FailureKind failureKind = FailureKind::General;

	[[nodiscard]]
	bool IsSuccess() const noexcept
	{
		return succeeded;
	}
};

}