#pragma once
#include <memory>
#include "Engine/Component/Camera.h"

class CameraSystem
{
public:
	CameraSystem() = default;	// Constructor
	~CameraSystem() = default;	// Destructor

	// Main processing functions
	void Initialize();				// Initialization
	void Update();					// Update
	void Flush(float deltaTime);	// Flush (reset camera system state if needed, called at the end of each frame)

	void SetMainCamera(Camera* camera);

	// Getters
	const CameraInfo* GetCameraInfo() const;						// Get camera information
	const Camera* GetMainCamera() const { return m_pMainCamera; }	// Get main camera component pointer

private:
	Camera* m_pMainCamera = nullptr;		// Main camera component pointer (for easy access)
};