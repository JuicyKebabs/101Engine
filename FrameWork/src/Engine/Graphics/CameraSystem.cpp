#include "Engine/Graphics/CameraSystem.h"
#include "Engine/Core/Debug/Debug.h"

void CameraSystem::Initialize()
{
}

void CameraSystem::Update()
{
}

void CameraSystem::Flush(float deltaTime)
{
	if (m_pMainCamera) {
		m_pMainCamera->Flush(deltaTime);
	}
}

void CameraSystem::SetMainCamera(Camera* camera)
{
	if (camera) {
		m_pMainCamera = camera;
	}
	else {
		DBG("CameraSystem::SetMainCamera() - Attempted to set main camera to a null pointer.\n");
	}
}
const CameraInfo* CameraSystem::GetCameraInfo() const
{
	if (m_pMainCamera) {
		return &m_pMainCamera->GetCameraInfo();
	}
	else {
		DBG("CameraSystem::GetCameraInfo() - No main camera set. Returning null pointer.\n");
		return nullptr;
	}
}