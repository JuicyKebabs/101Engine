#pragma once

class Transform;
struct InspectorContext;

//---------------------------------------------------------------------
// Editor-only inspector drawer for Transform.
//---------------------------------------------------------------------

class TransformInspector
{
public:
    static void Draw(
        Transform& transform,
        const InspectorContext& context
    );
};
