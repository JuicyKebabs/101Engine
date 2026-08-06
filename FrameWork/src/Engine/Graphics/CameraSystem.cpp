#include "Engine/Graphics/CameraSystem.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Debug/Debug.h"

void CameraSystem::Initialize()
{
}

void CameraSystem::Update()
{
}

void CameraSystem::Flush(float deltaTime)
{
	Camera* mainCamera = ResolveMainCamera();
	if (mainCamera)
	{
		mainCamera->Flush(deltaTime);
	}
}

bool CameraSystem::SetMainCamera(Camera* camera)
{
	if (!camera)
	{
		DBG("CameraSystem::SetMainCamera: Camera is null.");
		return false;
	}

	if (!m_scene)
	{
		DBG("CameraSystem::SetMainCamera: Scene is null.");
		return false;
	}

	Actor* owner = camera->GetOwner();

	if (!owner || owner->GetOwner() != m_scene)
	{
		DBG("CameraSystem::SetMainCamera: Camera does not belong to this Scene.");
		return false;
	}

	if (!m_mainCameraActor.Set(owner))
	{
		DBG("CameraSystem::SetMainCamera: Failed to reference the Camera Actor.");
		return false;
	}

	return true;
}

void CameraSystem::ClearMainCamera()
{
	m_mainCameraActor.Clear();
}

Camera* CameraSystem::ResolveMainCamera() const
{
	if (!m_scene) return nullptr;

	Actor* actor = m_mainCameraActor.Resolve(*m_scene);
	if (!actor) return nullptr;

	return actor->GetComponentByClass<Camera>();
}

const CameraInfo* CameraSystem::GetCameraInfo() const
{
	Camera* mainCamera = ResolveMainCamera();

	return mainCamera
		? &mainCamera->GetCameraInfo()
		: nullptr;
}

const Camera* CameraSystem::GetMainCamera() const
{
	return ResolveMainCamera();
}
