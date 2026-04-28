#include "Player.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/Collider.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Core/Time/Time.h"

using namespace DirectX;

// Scene-specific update
void PlayerBehavior::UpdateBehavior()
{
	Transform* transform = GetOwner()->GetComponentByClass<Transform>();

	//movement speed
	const float MOVE_SPEED = 0.2f;

	auto controllerLeftStick = InputManager::GetInstance().GetInputInfo().controller->leftStick;
	if (fabs(controllerLeftStick.x) < 0.1f) controllerLeftStick.x = 0.0f;
	if (fabs(controllerLeftStick.y) < 0.1f) controllerLeftStick.y = 0.0f;
	Vector3 currentPosition = transform->GetLocalPosition();
	Vector3 velocityInput = { controllerLeftStick.x, 0.0f, controllerLeftStick.y };
	float deltaTime = Time::GetTimeSeconds();
	currentPosition.x += velocityInput.x * MOVE_SPEED * deltaTime;
	currentPosition.y += velocityInput.y * MOVE_SPEED * deltaTime;
	currentPosition.z += velocityInput.z * MOVE_SPEED * deltaTime;
	transform->SetLocalPosition(currentPosition);

	auto controllerRightStick = InputManager::GetInstance().GetInputInfo().controller->rightStick;
	if (fabs(controllerRightStick.x) < 0.1f) controllerRightStick.x = 0.0f;
	if (fabs(controllerRightStick.y) < 0.1f) controllerRightStick.y = 0.0f;
	Quaternion currentRotation = transform->GetLocalRotationQuat();
	Vector3 rotationInput = { controllerRightStick.y, -controllerRightStick.x, 0.0f };
	transform->RotateLocalByEulerDeg(rotationInput);

	auto controllerRdown = InputManager::GetInstance().GetInputInfo().controller->RSHOULDER.down;
	if (controllerRdown)
	{
		Vector3 currentScale = transform->GetLocalScale();
		currentScale.x += 0.1f;
		currentScale.y += 0.1f;
		currentScale.z += 0.1f;
		transform->SetLocalScale(currentScale);
	}
	auto controllerLdown = InputManager::GetInstance().GetInputInfo().controller->LSHOULDER.down;
	if (controllerLdown)
	{
		Vector3 currentScale = transform->GetLocalScale();
		currentScale.x = (std::max)(currentScale.x - 0.1f, 0.0f);
		currentScale.y = (std::max)(currentScale.y - 0.1f, 0.0f);
		currentScale.z = (std::max)(currentScale.z - 0.1f, 0.0f);
		transform->SetLocalScale(currentScale);
	}

	auto keyInput = InputManager::GetInstance().GetInputInfo().key;
	auto downW = keyInput.w.down;
	auto downA = keyInput.a.down;
	auto downS = keyInput.s.down;
	auto downD = keyInput.d.down;

	const float ROTATION_SPEED = 1.0f; // degrees per second

	if (downW)
	{
		Vector3 currentPosition = transform->GetLocalPosition();
		currentPosition.y += MOVE_SPEED;
		transform->SetLocalPosition(currentPosition);
		//transform->RotateLocalByEulerDeg({ -ROTATION_SPEED, 0.0f, 0.0f });
	}
	if (downA)
	{
		Vector3 currentPosition = transform->GetLocalPosition();
		currentPosition.x -= MOVE_SPEED;
		transform->SetLocalPosition(currentPosition);
		//transform->RotateLocalByEulerDeg({ 0.0f, -ROTATION_SPEED, 0.0f });
	}
	if (downS)
	{
		Vector3 currentPosition = transform->GetLocalPosition();
		currentPosition.y -= MOVE_SPEED;
		transform->SetLocalPosition(currentPosition);
		//transform->RotateLocalByEulerDeg({ ROTATION_SPEED, 0.0f, 0.0f });
	}
	if (downD)
	{
		Vector3 currentPosition = transform->GetLocalPosition();
		currentPosition.x += MOVE_SPEED;
		transform->SetLocalPosition(currentPosition);
		//transform->RotateLocalByEulerDeg({ 0.0f, ROTATION_SPEED, 0.0f });
	}

	//auto downUp = keyInput.up.down;
	//auto downLeft = keyInput.left.down;
	//auto downDown = keyInput.down.down;
	//auto downRight = keyInput.right.down;

	//if (downUp)
	//{
	//	transform->RotateLocalByXDeg(-1.0f);
	//}
	//if (downLeft)
	//{
	//	transform->RotateLocalByYDeg(-1.0f);
	//}
	//if (downDown)
	//{
	//	transform->RotateLocalByXDeg(1.0f);
	//}
	//if (downRight)
	//{
	//	transform->RotateLocalByYDeg(1.0f);
	//}

	auto downQ = keyInput.q.down;
	auto downE = keyInput.e.down;
	if (downQ)
	{
		Vector3 currentScale = transform->GetLocalScale();
		currentScale.x = (std::max)(currentScale.x - 0.1f, 0.0f);
		currentScale.y = (std::max)(currentScale.y - 0.1f, 0.0f);
		currentScale.z = (std::max)(currentScale.z - 0.1f, 0.0f);
		transform->SetLocalScale(currentScale);
	}
	if (downE)
	{
		Vector3 currentScale = transform->GetLocalScale();
		currentScale.x += 0.1f;
		currentScale.y += 0.1f;
		currentScale.z += 0.1f;
		transform->SetLocalScale(currentScale);
	}

	// Controller vibration test
	if (controllerLeftStick.x < 0.2f)
	{
		InputManager::GetInstance().SetAllControllerVibrations(
			fabs(controllerLeftStick.x),
			0.0f
		);
	}
	else if (controllerLeftStick.x > 0.2f)
	{
		InputManager::GetInstance().SetAllControllerVibrations(
			0.0f,
			controllerLeftStick.x
		);
	}
	else
	{
		InputManager::GetInstance().StopAllControllerVibrations();
	}
}

void PlayerBehavior::LateUpdateBehavior()
{
	auto collider = GetOwner()->GetComponentByClass<Collider>();
	if (collider)
	{
		auto renderer = GetOwner()->GetComponentByClass<MeshRenderer>();
		if (!renderer)
		{
			DBG("PlayerBehavior: Renderer component not found on the actor.");
			return;
		}

		if (collider->isDetected())
		{
			renderer->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f }); // Red color when collision is detected

			auto pushBack = collider->GetCollisionInfos()[0].penetrationDepth * 1.001f; // Get the push back vector from the first collision info

			auto transform = GetOwner()->GetTransform();
			transform->SetLocalPosition(transform->GetLocalPosition() - pushBack); // Move the player back by the push back vector to resolve the collision
		}
		else
		{
			renderer->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // Default color when no collision is detected
		}
	}
}