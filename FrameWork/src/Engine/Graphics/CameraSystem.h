#pragma once
#include <memory>
#include "Engine/Actor/ActorReference.h"
#include "Engine/Component/Camera.h"

class SceneBase;

//----------------------------------------------------------
// CameraSystem class
// A system that manages main camera component in the scene.
//----------------------------------------------------------

class CameraSystem
{
public:
	explicit CameraSystem(SceneBase* scene) : m_scene(scene) {}

	// Main processing functions
	void Initialize();				// Initialization
	void Update();					// Update
	void Flush(float deltaTime);	// Flush (reset camera system state if needed, called at the end of each frame)

	bool SetMainCamera(Camera* camera);
	void ClearMainCamera();

	// Getters
	const CameraInfo* GetCameraInfo() const;	// Get camera information
	const Camera* GetMainCamera() const;		// Get main camera component pointer

private:
	SceneBase* m_scene = nullptr;
	ActorReference m_mainCameraActor;

private:
	Camera* ResolveMainCamera() const;
};
