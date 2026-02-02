#include "Player.h"
#include "InputManager.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "Audio.h"
#include "Debug.h"

using namespace DirectX;
// Constructor
Player::Player(
	MESH_TYPE meshType,
	XMFLOAT3 position, 
	XMFLOAT3 rotation,
	XMFLOAT3 scale,
	XMFLOAT3 velocity, 
	bool isActive, 
	OBJECT_TAG tag, 
	CollisionData::COLLISION_LAYER layer,
	XMFLOAT3 colliderSetScale, 
	XMFLOAT3 colliderSetOffsetPosition, 
	XMFLOAT3 colliderSetOffsetRotation
)
	: ObjectBase(
		meshType,
		position,
		rotation,
		scale,
		velocity,
		isActive,
		tag,
		layer,
		colliderSetScale,
		colliderSetOffsetPosition,
		colliderSetOffsetRotation
	)
{
	XMFLOAT3 colliderScale =
	{
		scale.x * 1.2f,
		scale.y * 1.2f,
		scale.z * 1.2f
	};

	GetColliderSet()->AddCollider(
		ColliderType::BOX,
		{ 0.0f, 0.0f, 0.0f },
		colliderScale,
		{ 0.0f, 0.0f, 0.0f }
	);
}

// Destructor
Player::~Player()
{
}

// Create render model
void Player::CreateRenderModel(
	TextureManager& pTextureManager,
	MeshManager& pMeshManager
)
{
	WorldRenderModel info;
	CreateRenderInfo(
		pTextureManager,				// Texture manager reference
		pMeshManager,					// Mesh manager reference
		&info,							// Pointer to the render info structure array
		GetMeshType(),					// Mesh type
		BLEND_OPAQUE,					// Blend mode
		L"asset/fbx/screw/ST_screw.fbx",	// Model data or texture file path
		true,							// Whether lighting is enabled
		BILLBOARD_TYPE::BILLBOARD_NONE,	// Billboard type
		false,							// Whether to flip U (only valid for model data)
		false							// Whether to flip V (only valid for model data)
	);
	SetRenderModel(info);
	auto animatorSet = GetNodeAnimatorSet();
	animatorSet->pNodeAnimator->Bind(GetRenderModel()[0].pNodeAnimAsset);
	animatorSet->isAnimLoaded = true;
}

// Scene-specific update
void Player::UpdateOverride()
{
	//movement speed
	const float MOVE_SPEED = 0.2f;

	auto controllerLeftStick = InputManager::GetInstance()->GetInputInfo()->controller->leftStick;
	if (fabs(controllerLeftStick.x) < 0.1f) controllerLeftStick.x = 0.0f;
	if (fabs(controllerLeftStick.y) < 0.1f) controllerLeftStick.y = 0.0f;
	XMFLOAT3 currentPosition = GetPosition();
	XMFLOAT3 velocityInput = { controllerLeftStick.x, 0.0f, controllerLeftStick.y };
	currentPosition.x += velocityInput.x * MOVE_SPEED;
	currentPosition.y += velocityInput.y * MOVE_SPEED;
	currentPosition.z += velocityInput.z * MOVE_SPEED;
	SetPosition(currentPosition);

	auto controllerRightStick = InputManager::GetInstance()->GetInputInfo()->controller->rightStick;
	if (fabs(controllerRightStick.x) < 0.1f) controllerRightStick.x = 0.0f;
	if (fabs(controllerRightStick.y) < 0.1f) controllerRightStick.y = 0.0f;
	XMFLOAT3 currentRotation = GetRotation();
	XMFLOAT3 rotationInput = { controllerRightStick.y, -controllerRightStick.x, 0.0f };
	currentRotation.x += rotationInput.x;
	currentRotation.y += rotationInput.y;
	currentRotation.z += rotationInput.z;
	SetRotation(currentRotation);

	auto controllerRdown = InputManager::GetInstance()->GetInputInfo()->controller->RSHOULDER.down;
	if (controllerRdown)
	{
		XMFLOAT3 currentScale = GetScale();
		currentScale.x += 0.1f;
		currentScale.y += 0.1f;
		currentScale.z += 0.1f;
		SetScale(currentScale);
	}
	auto controllerLdown = InputManager::GetInstance()->GetInputInfo()->controller->LSHOULDER.down;
	if (controllerLdown)
	{
		XMFLOAT3 currentScale = GetScale();
		currentScale.x = (std::max)(currentScale.x - 0.1f, 0.0f);
		currentScale.y = (std::max)(currentScale.y - 0.1f, 0.0f);
		currentScale.z = (std::max)(currentScale.z - 0.1f, 0.0f);
		SetScale(currentScale);
	}

	auto keyInput = InputManager::GetInstance()->GetInputInfo()->key;
	auto downW = keyInput.w.down;
	auto downA = keyInput.a.down;
	auto downS = keyInput.s.down;
	auto downD = keyInput.d.down;
	if (downW)
	{
		XMFLOAT3 currentPosition = GetPosition();
		currentPosition.z += MOVE_SPEED;
		SetPosition(currentPosition);
	}
	if (downA)
	{
		XMFLOAT3 currentPosition = GetPosition();
		currentPosition.x -= MOVE_SPEED;
		SetPosition(currentPosition);
	}
	if (downS)
	{
		XMFLOAT3 currentPosition = GetPosition();
		currentPosition.z -= MOVE_SPEED;
		SetPosition(currentPosition);
	}
	if (downD)
	{
		XMFLOAT3 currentPosition = GetPosition();
		currentPosition.x += MOVE_SPEED;
		SetPosition(currentPosition);
	}

	auto downUp = keyInput.up.down;
	auto downLeft = keyInput.left.down;
	auto downDown = keyInput.down.down;
	auto downRight = keyInput.right.down;

	if (downUp)
	{
		XMFLOAT3 currentRotation = GetRotation();
		currentRotation.x -= 1.0f;
		SetRotation(currentRotation);
	}
	if (downLeft)
	{
		XMFLOAT3 currentRotation = GetRotation();
		currentRotation.y += 1.0f;
		SetRotation(currentRotation);
	}
	if (downDown)
	{
		XMFLOAT3 currentRotation = GetRotation();
		currentRotation.x += 1.0f;
		SetRotation(currentRotation);
	}
	if (downRight)
	{
		XMFLOAT3 currentRotation = GetRotation();
		currentRotation.y -= 1.0f;
		SetRotation(currentRotation);
	}

	auto downQ = keyInput.q.down;
	auto downE = keyInput.e.down;
	if (downQ)
	{
		XMFLOAT3 currentScale = GetScale();
		currentScale.x = (std::max)(currentScale.x - 0.1f, 0.0f);
		currentScale.y = (std::max)(currentScale.y - 0.1f, 0.0f);
		currentScale.z = (std::max)(currentScale.z - 0.1f, 0.0f);
		SetScale(currentScale);
	}
	if (downE)
	{
		XMFLOAT3 currentScale = GetScale();
		currentScale.x += 0.1f;
		currentScale.y += 0.1f;
		currentScale.z += 0.1f;
		SetScale(currentScale);
	}

	auto animatorSet = GetNodeAnimatorSet();
	animatorSet->isAnimPlaying = true;	

	if(keyInput.space.down)
	{
		const double MAX_ANIM_TIME = 1.0 / 30.0; //slowest speed
		m_animTime = (std::min)(m_animTime * 1.05, MAX_ANIM_TIME); //fast speed
	}
	else
	{
		m_animTime = (std::max)(m_animTime * 0.95, ANIM_TIME); //slow speed
	}

	animatorSet->pNodeAnimator->Update(m_animTime);

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

// Scene-specific collision resolution
void Player::ResolveCollisionsOverride()
{
}