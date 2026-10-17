#pragma once

#include "Result.h"

namespace rendering
{

enum class RenderDisposition
{
	NotProcessed,
	SkippedNoSubmission,
	Processed,	// Currently upload preparation and optional Clear / Present; Object drawing comes later.
};

struct [[nodiscard]] RenderResult
{
	Result status;
	RenderDisposition disposition = RenderDisposition::NotProcessed;
};

}
