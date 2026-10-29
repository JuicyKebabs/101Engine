// R7 implementation guide: section 4.37.
// Convert actual Engine CPU Vertex fields to the Renderer's existing MeshVertex representation.
// Registration copies the converted arrays before these local vectors are destroyed.
// This demonstrates the public Geometry boundary, not the complete R8 migration.

#include "EngineMeshBridge.h"
#include "Engine/Graphics/RenderData.h"

namespace rendering::probes
{

// Explicit field conversion avoids assuming two different Vertex types share binary layout.
// GeometryDesc borrows these arrays only during RegisterGeometry; the Renderer copies them before return.
GeometryRegistrationResult RegisterEngineMesh(Renderer& renderer, const ::Mesh& mesh)
{
	std::vector<MeshVertex> vertices;
	vertices.reserve(mesh.vertices.size());

	for (const auto& source : mesh.vertices)
	{
		MeshVertex vertex;
		vertex.position.x = source.position.x;
		vertex.position.y = source.position.y;
		vertex.position.z = source.position.z;
		vertex.normal.x = source.normal.x;
		vertex.normal.y = source.normal.y;
		vertex.normal.z = source.normal.z;
		vertex.uv.x = source.uv.x;
		vertex.uv.y = source.uv.y;
		vertex.tangent.x = source.tangent.x;
		vertex.tangent.y = source.tangent.y;
		vertex.tangent.z = source.tangent.z;
		vertex.color.x = source.color.x;
		vertex.color.y = source.color.y;
		vertex.color.z = source.color.z;
		vertex.color.w = source.color.w;
		vertices.push_back(vertex);
	}

	GeometryDesc description;
	description.vertices = vertices;
	description.indices = mesh.indices;
	return renderer.RegisterGeometry(description);
}

// Use the Engine's real built-in CPU Quad through its real Mesh type.
// This proves the public CPU Geometry bridge without linking MeshGPU or migrating Engine components.
GeometryRegistrationResult RegisterEngineQuad(Renderer& renderer)
{
	// Use the actual Engine Mesh type and its built-in CPU vertex/index data.
	// No Engine Device, MeshGPU, Asset GUID table or old CommandList is shared.
	::Mesh mesh;
	mesh.vertices.reserve(std::size(QuadVertices));
	for (const auto& vertex : QuadVertices)
	{
		mesh.vertices.push_back(vertex);
	}
	mesh.indices.assign(std::begin(QuadIndices), std::end(QuadIndices));
	mesh.vertexCount = mesh.vertices.size();
	mesh.indexCount = mesh.indices.size();
	return RegisterEngineMesh(renderer, mesh);
}

}
