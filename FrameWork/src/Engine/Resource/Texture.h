#pragma once
#include "Renderer/ResourceHandle.h"

// A CPU-upload Texture identity only. Generated outputs keep their distinct
// RenderTargetTextureHandle type; no descriptor index is an Engine texture ID.
using TextureHandle = rendering::TextureHandle;
inline constexpr TextureHandle InvalidTextureHandle;
