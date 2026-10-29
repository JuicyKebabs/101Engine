#pragma once

#include "Result.h"
#include "DrawTypes.h"

//-----------------------------------------------------------------------------------
// RenderResult.h
// Result of a single Render() call, including status, disposition, and diagnostics.
//-----------------------------------------------------------------------------------

namespace rendering
{

enum class RenderDisposition
{
	NotProcessed,
	SkippedNoSubmission,
	Processed,	// Submission consumed; inspect drawnObjects and diagnostics separately.
};

struct [[nodiscard]] RenderResult
{
	Result status;
	RenderDisposition disposition = RenderDisposition::NotProcessed;
	std::size_t drawnObjects = 0;
	std::vector<DrawDiagnostic> diagnostics;
};

}
