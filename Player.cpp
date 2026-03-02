#include "Player.h"
#include "InputManager.h"
#include "Actor.h"
#include "TransformComponent.h"
#include "Debug.h"

using namespace DirectX;

// Scene-specific update
void PlayerBehavior::Update(float deltaTime)
{
	TransformComponent* transform = GetOwner()->GetComponent<TransformComponent>();

	//movement speed
	const float MOVE_SPEED = 0.2f;

	auto controllerLeftStick = InputManager::GetInstance()->GetInputInfo()->controller->leftStick;
	if (fabs(controllerLeftStick.x) < 0.1f) controllerLeftStick.x = 0.0f;
	if (fabs(controllerLeftStick.y) < 0.1f) controllerLeftStick.y = 0.0f;
	XMFLOAT3 currentPosition = transform->GetPosition();
	XMFLOAT3 velocityInput = { controllerLeftStick.x, 0.0f, controllerLeftStick.y };
	currentPosition.x += velocityInput.x * MOVE_SPEED;
	currentPosition.y += velocityInput.y * MOVE_SPEED;
	currentPosition.z += velocityInput.z * MOVE_SPEED;
	transform->SetPosition(currentPosition);

	auto controllerRightStick = InputManager::GetInstance()->GetInputInfo()->controller->rightStick;
	if (fabs(controllerRightStick.x) < 0.1f) controllerRightStick.x = 0.0f;
	if (fabs(controllerRightStick.y) < 0.1f) controllerRightStick.y = 0.0f;
	XMFLOAT3 currentRotation = transform->GetRotation();
	XMFLOAT3 rotationInput = { controllerRightStick.y, -controllerRightStick.x, 0.0f };
	currentRotation.x += rotationInput.x;
	currentRotation.y += rotationInput.y;
	currentRotation.z += rotationInput.z;
	transform->SetRotation(currentRotation);

	auto controllerRdown = InputManager::GetInstance()->GetInputInfo()->controller->RSHOULDER.down;
	if (controllerRdown)
	{
		XMFLOAT3 currentScale = transform->GetScale();
		currentScale.x += 0.1f;
		currentScale.y += 0.1f;
		currentScale.z += 0.1f;
		transform->SetScale(currentScale);
	}
	auto controllerLdown = InputManager::GetInstance()->GetInputInfo()->controller->LSHOULDER.down;
	if (controllerLdown)
	{
		XMFLOAT3 currentScale = transform->GetScale();
		currentScale.x = (std::max)(currentScale.x - 0.1f, 0.0f);
		currentScale.y = (std::max)(currentScale.y - 0.1f, 0.0f);
		currentScale.z = (std::max)(currentScale.z - 0.1f, 0.0f);
		transform->SetScale(currentScale);
	}

	auto keyInput = InputManager::GetInstance()->GetInputInfo()->key;
	auto downW = keyInput.w.down;
	auto downA = keyInput.a.down;
	auto downS = keyInput.s.down;
	auto downD = keyInput.d.down;
	if (downW)
	{
		XMFLOAT3 currentPosition = transform->GetPosition();
		currentPosition.z += MOVE_SPEED;
		transform->SetPosition(currentPosition);
	}
	if (downA)
	{
		XMFLOAT3 currentPosition = transform->GetPosition();
		currentPosition.x -= MOVE_SPEED;
		transform->SetPosition(currentPosition);
	}
	if (downS)
	{
		XMFLOAT3 currentPosition = transform->GetPosition();
		currentPosition.z -= MOVE_SPEED;
		transform->SetPosition(currentPosition);
	}
	if (downD)
	{
		XMFLOAT3 currentPosition = transform->GetPosition();
		currentPosition.x += MOVE_SPEED;
		transform->SetPosition(currentPosition);
	}

	auto downUp = keyInput.up.down;
	auto downLeft = keyInput.left.down;
	auto downDown = keyInput.down.down;
	auto downRight = keyInput.right.down;

	if (downUp)
	{
		XMFLOAT3 currentRotation = transform->GetRotation();
		currentRotation.x -= 1.0f;
		transform->SetRotation(currentRotation);
	}
	if (downLeft)
	{
		XMFLOAT3 currentRotation = transform->GetRotation();
		currentRotation.y += 1.0f;
		transform->SetRotation(currentRotation);
	}
	if (downDown)
	{
		XMFLOAT3 currentRotation = transform->GetRotation();
		currentRotation.x += 1.0f;
		transform->SetRotation(currentRotation);
	}
	if (downRight)
	{
		XMFLOAT3 currentRotation = transform->GetRotation();
		currentRotation.y -= 1.0f;
		transform->SetRotation(currentRotation);
	}

	auto downQ = keyInput.q.down;
	auto downE = keyInput.e.down;
	if (downQ)
	{
		XMFLOAT3 currentScale = transform->GetScale();
		currentScale.x = (std::max)(currentScale.x - 0.1f, 0.0f);
		currentScale.y = (std::max)(currentScale.y - 0.1f, 0.0f);
		currentScale.z = (std::max)(currentScale.z - 0.1f, 0.0f);
		transform->SetScale(currentScale);
	}
	if (downE)
	{
		XMFLOAT3 currentScale = transform->GetScale();
		currentScale.x += 0.1f;
		currentScale.y += 0.1f;
		currentScale.z += 0.1f;
		transform->SetScale(currentScale);
	}

	// Controller vibration test
	if (controllerLeftStick.x < 0.2f)
	{
		InputManager::GetInstance()->SetAllControllerVibrations(
			fabs(controllerLeftStick.x),
			0.0f
		);
	}
	else if (controllerLeftStick.x > 0.2f)
	{
		InputManager::GetInstance()->SetAllControllerVibrations(
			0.0f,
			controllerLeftStick.x
		);
	}
	else
	{
		InputManager::GetInstance()->StopAllControllerVibrations();
	}

}