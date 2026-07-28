#pragma once

class Camera;
struct InspectorContext;

//-------------------------------------------------------------------------------------
// CameraInspector class
// This class is responsible for rendering the inspector UI for Camera component
//-------------------------------------------------------------------------------------

class CameraInspector
{
public:
	static void Draw(Camera& camera, const InspectorContext& context);
};
