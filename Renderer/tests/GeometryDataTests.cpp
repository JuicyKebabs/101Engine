// R8: exercise the public CPU contract independently from GPU upload timing.
// Failed updates and failed queries must not destroy the last accepted snapshot.
#include "Renderer/Renderer.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{

bool Check(bool condition, const char* purpose)
{
    if (!condition)
    {
        std::cerr << purpose << '\n';
    }

    return condition;
}

}

int main()
{
    rendering::Renderer renderer;
    rendering::RendererConfiguration configuration;
    if (!Check(renderer.Initialize(configuration).IsSuccess(), "Initialize Renderer"))
    {
        return 1;
    }

    std::array<rendering::MeshVertex, 3> vertices;
    vertices[0].position.x = -2;
    vertices[1].position.x = 2;
    vertices[2].position.y = 3;

    rendering::GeometryDesc description;
    description.vertices = vertices;
    rendering::GeometryRegistrationResult registration = renderer.RegisterGeometry(description);
    if (!Check(registration.status.IsSuccess(), "Accept CPU Geometry before upload"))
    {
        return 1;
    }

    rendering::GeometryData firstSnapshot;
    if (!Check(renderer.GetGeometryData(registration.handle, firstSnapshot).IsSuccess(), "Query pending Geometry without GPU wait"))
    {
        return 1;
    }

    bool valid = Check(firstSnapshot.revision == 1, "First revision equals one");
    valid &= Check(firstSnapshot.boundsCenter.x == 0, "Bounds center X");
    valid &= Check(firstSnapshot.boundsCenter.y == 1.5f, "Bounds center Y");
    valid &= Check(firstSnapshot.boundsRadius >= 2.5f, "Bounds conservatively enclose triangle");

    vertices[1].position.x = 20;
    valid &= Check(renderer.UpdateGeometry(registration.handle, description).IsSuccess(), "Accept larger Geometry");

    rendering::GeometryData secondSnapshot;
    valid &= Check(renderer.GetGeometryData(registration.handle, secondSnapshot).IsSuccess(), "Query replacement Content");
    valid &= Check(secondSnapshot.revision == 2, "Successful update increments revision");
    valid &= Check(secondSnapshot.boundsRadius > firstSnapshot.boundsRadius, "Bounds and revision update together");

    vertices[0].position.x = (std::numeric_limits<float>::quiet_NaN)();
    valid &= Check(!renderer.UpdateGeometry(registration.handle, description).IsSuccess(), "Reject nonfinite update");

    rendering::GeometryData preservedSnapshot;
    valid &= Check(renderer.GetGeometryData(registration.handle, preservedSnapshot).IsSuccess(), "Old Content survives failed update");
    valid &= Check(preservedSnapshot.revision == secondSnapshot.revision, "Failed update preserves revision");
    valid &= Check(preservedSnapshot.boundsRadius == secondSnapshot.boundsRadius, "Failed update preserves Bounds");

    valid &= Check(renderer.DestroyGeometry(registration.handle).IsSuccess(), "Request Geometry destruction");
    valid &= Check(!renderer.GetGeometryData(registration.handle, preservedSnapshot).IsSuccess(), "Reject query after Destroy request");
    valid &= Check(preservedSnapshot.revision == secondSnapshot.revision, "Failed query leaves destination untouched");

    renderer.Finalize();
    return valid ? 0 : 1;
}
