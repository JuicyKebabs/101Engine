// R7 implementation guide: section 4.36.
// Narrow Engine-to-Renderer CPU Mesh conversion probe, outside the Renderer core.
// No Engine Device, MeshGPU, Asset identity or Component extraction is migrated here.

#pragma once

#include "Renderer/Renderer.h"

struct Mesh;

namespace rendering::probes
{

GeometryRegistrationResult RegisterEngineMesh(Renderer& renderer, const ::Mesh& mesh);
GeometryRegistrationResult RegisterEngineQuad(Renderer& renderer);

}
