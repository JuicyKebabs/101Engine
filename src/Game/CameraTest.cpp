#include "CameraTest.h"
#include "Engine/Component/Camera.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Actor/Actor.h"

void CameraTest::OnStartBehavior()
{
	m_isMainCamera = false;
}

void CameraTest::UpdateBehavior()
{
	auto inputInfo = InputManager::GetInstance().GetInputInfo();

	if (inputInfo.key.space.trigger){
		auto camera = GetOwner()->GetComponentByClass<Camera>();
		if (camera) {
			camera->SetAsMainCamera();
			camera->SetRotationMode(CAMERA_ROTATION_MODE::ROTATION_MODE_MATCH_OWNER);
			m_isMainCamera = true;
		}
	}

	if (!m_isMainCamera) return;

	auto transform = GetOwner()->GetComponentByClass<Transform>();
	if (transform) {

		const float rotationSpeed = 1.0f; // degrees per second

		if(inputInfo.key.up.down) {
			transform->RotateLocalByEulerDeg({ -rotationSpeed, 0.0f, 0.0f });
		}
		if(inputInfo.key.down.down) {
			transform->RotateLocalByEulerDeg({ rotationSpeed, 0.0f, 0.0f });
		}
		if(inputInfo.key.left.down) {
			transform->RotateLocalByEulerDeg({ 0.0f, -rotationSpeed, 0.0f });
		}
		if(inputInfo.key.right.down) {
			transform->RotateLocalByEulerDeg({ 0.0f, rotationSpeed, 0.0f });
		}

		const float moveSpeed = 0.1f; // units per second

		if(inputInfo.key.i.down) {
			auto forward = transform->GetLocalForward();
			transform->SetLocalPosition(transform->GetLocalPosition() + forward * moveSpeed);
		}
		if(inputInfo.key.k.down) {
			auto backward = transform->GetLocalBack();
			transform->SetLocalPosition(transform->GetLocalPosition() + backward * moveSpeed);
		}
		if(inputInfo.key.j.down) {
			auto left = transform->GetLocalLeft();
			transform->SetLocalPosition(transform->GetLocalPosition() + left * moveSpeed);
		}
		if(inputInfo.key.l.down) {
			auto right = transform->GetLocalRight();
			transform->SetLocalPosition(transform->GetLocalPosition() + right * moveSpeed);
		}
	}
}
