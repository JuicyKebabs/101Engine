#pragma once
#include "Engine/Core/Math/Math.h"

// ---------------------------------------------------------------------------
// UV rectangle (raw texture region, not tied to split logic)
// ---------------------------------------------------------------------------
struct UVRect
{
	float u  = 0.0f;	// Left edge in UV space
	float v  = 0.0f;	// Top edge in UV space
	float su = 1.0f;	// Width in UV space
	float sv = 1.0f;	// Height in UV space
};

// ---------------------------------------------------------------------------
// Sprite sheet split descriptor
// Describes how to index into a sprite atlas and advance frames.
// ---------------------------------------------------------------------------
struct TexSplitInfo
{
	int   index       = 0;		// Current sprite index (0-based)
	int   cols        = 1;		// Number of columns in the atlas
	int   rows        = 1;		// Number of rows in the atlas
	int   total       = 1;		// Total frame count (cols * rows)
	int   frameCount  = 0;		// Elapsed frame counter (incremented by caller)
	int   updateRate  = 0;		// How many frames to wait before advancing index

	float offsetU = 0.0f;		// Additional UV offset within a cell (U)
	float offsetV = 0.0f;		// Additional UV offset within a cell (V)
	float scaleU  = 1.0f;		// UV scale within a cell (U)
	float scaleV  = 1.0f;		// UV scale within a cell (V)
};

// Compute the UV sub-rectangle for the current frame of a sprite sheet.
// Returns Vector4(minU, minV, sizeU, sizeV) suitable for SpriteRenderConstants::uvRect.
inline Vector4 SplitSprite(TexSplitInfo info)
{
	const float baseSu = 1.0f / static_cast<float>(info.cols);
	const float baseSv = 1.0f / static_cast<float>(info.rows);

	const int col = info.index % info.cols;
	const int row = info.index / info.cols;

	const float frameU = static_cast<float>(col) * baseSu;
	const float frameV = static_cast<float>(row) * baseSv;

	const float minU  = frameU + info.offsetU * baseSu;
	const float minV  = frameV + info.offsetV * baseSv;
	const float sizeU = baseSu * info.scaleU;
	const float sizeV = baseSv * info.scaleV;

	return Vector4(minU, minV, sizeU, sizeV);
}
