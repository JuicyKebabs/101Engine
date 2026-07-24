#include <cmath>
#include "Collider.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Core/Serialization/JsonMath.h"
#include "nlohmann/json.hpp"

void Collider::OnStartOverride()
{
	m_layerMask = MakeLayerMask(m_layer);

	auto owner = GetOwner();
	if (owner)
	{
		m_ownerTag = owner->GetTag();
		auto ownerScene = owner->GetOwner();
		if (ownerScene)
		{
			ownerScene->GetCollisionSystem()->Register(this);
		}
		else
		{
			DBG("Collider : Owner scene is null.");
		}
	}
	else
	{
		DBG("Collider : Owner actor is null.");
	}
}

void Collider::PreUpdateOverride(float deltaTime)
{
}

void Collider::UpdateOverride(float deltaTime)
{
}

void Collider::LateUpdateOverride(float deltaTime)
{
	RefreshWorldTransform();
}

void Collider::OnDestroyOverride()
{
	m_isActive = false;
	m_deleteFlag = true;
}

void Collider::Flush()
{
	RefreshWorldTransform();
}

void Collider::UpdateCollider(Vector3 ownerScale)
{
	switch (m_type)
	{
	case ColliderType::None:
		return;
		break;
	case ColliderType::BOX:
		UpdateBoxCollider(ownerScale);
		break;
	case ColliderType::SPHERE:
		UpdateSphereCollider(ownerScale);
		break;
	case ColliderType::CAPSULE:
		UpdateCapsuleCollider(ownerScale);
		break;
	default:
		break;
	}
}

void Collider::UpdateAABB()
{
	switch (m_type)
	{
	case ColliderType::BOX:
		UpdateAABBBox();
		break;
	case ColliderType::SPHERE:
		UpdateAABBSphere();
		break;
	case ColliderType::CAPSULE:
		UpdateAABBCapsule();
		break;
	default:
		break;
	}
	
	// Update the swept AABB for continuous collision detection
	MakeSweptAABB();
}

void Collider::SetPreviousState()
{
		m_previousBoxCollider = m_currentBoxCollider;			// Save previous box collider
	m_previousSphereCollider = m_currentSphereCollider;		// Save previous sphere collider
	m_previousCapsuleCollider = m_currentCapsuleCollider;	// Save previous capsule collider
	m_previousAABB = m_currentAABB;							// Save previous AABB
	m_worldTransformPrevious = m_worldTransformCurrent;		// Save previous world transform
}

void Collider::ChackIfTransformChanged()
{
	auto owner = GetOwner();
	if (owner) {
		auto transform = owner->GetComponentByClass<Transform>();
		if (transform) {
			uint64_t currentGeneration = transform->GetWorldGeneration();
			if (m_transformGeneration != currentGeneration) {
				m_transformGeneration = currentGeneration;
				m_isDirty = true;
			}
		}
	}
}

void Collider::RefreshWorldTransform()
{
	ChackIfTransformChanged();

	if (!m_isDirty) return;

	auto owner = GetOwner();
	if (!owner)
	{
		DBG("Collider : Owner actor is null.");
		return;
	}
	auto ownerTransform = owner->GetComponentByClass<Transform>();
	if (!ownerTransform)
	{
		DBG("Collider : Owner transform form is null.");
		return;
	}

	m_worldTransformCurrent.position = Vector3::Transform(m_localTransform.position, ownerTransform->GetWorldMatrix());
	m_worldTransformCurrent.rotation = m_localTransform.rotation * ownerTransform->GetWorldRotationQuat();
	UpdateCollider(ownerTransform->GetWorldScale());
	UpdateAABB();

	m_isDirty = false;
}

void Collider::UpdateBoxCollider(Vector3 ownerScale)
{
	// Update box collider center position
	m_currentBoxCollider.center = m_worldTransformCurrent.position;

	// Reflect scale
	m_currentBoxCollider.scale =
	{
		ownerScale.x * m_localTransform.scale.x,
		ownerScale.y * m_localTransform.scale.y,
		ownerScale.z * m_localTransform.scale.z
	};

	// Update world transform scale
	m_worldTransformCurrent.scale = m_currentBoxCollider.scale;
}

void Collider::UpdateSphereCollider(Vector3 ownerScale)
{
	// Update sphere collider center position
	m_currentSphereCollider.center = m_worldTransformCurrent.position;

	// Reflect scale
	Vector3 scale =
	{
		ownerScale.x * m_localTransform.scale.x,
		ownerScale.y * m_localTransform.scale.y,
		ownerScale.z * m_localTransform.scale.z
	};

	// Calculate diameter and radius
	const float diamiter = (std::max)(scale.x, (std::max)(scale.y, scale.z));
	const float radius = diamiter / 2.0f;

	m_worldTransformCurrent.scale = { diamiter, diamiter, diamiter };

	// Update sphere collider radius
	m_currentSphereCollider.radius = radius;
}

void Collider::UpdateCapsuleCollider(Vector3 ownerScale)
{
	// Reflect scale
	const Vector3 scale = 
	{
		ownerScale.x * m_localTransform.scale.x,
		ownerScale.y * m_localTransform.scale.y,
		ownerScale.z * m_localTransform.scale.z
	};

	// Calculate diameter, radius, and heights
	const float diamiter = (std::max)(scale.x, scale.z);
	const float radius = diamiter / 2.0f;
	const float capusleHeight = scale.y;
	const float cylHeight = (std::max)(0.0f, capusleHeight - diamiter);

	// Update capsule collider parameters
	m_currentCapsuleCollider.radius = radius;		// Update capsule collider radius
	m_currentCapsuleCollider.cylHeight = cylHeight;	// Update capsule collider height
	float halfHeight = cylHeight * 0.5f;			// Half of the cylinder height

	// Calculate the rotated axis based on the current world rotation
	Vector3 rotatedAxis = m_worldTransformCurrent.rotation.RotateVector3(Vector3::Up()).Normalized();
	Vector3 pA = m_worldTransformCurrent.position + rotatedAxis * halfHeight;
	Vector3 pB = m_worldTransformCurrent.position - rotatedAxis * halfHeight;

	// Update capsule collider endpoints
	m_currentCapsuleCollider.pointA = pA;
	m_currentCapsuleCollider.pointB = pB;

	// Update world transform scale
	m_worldTransformCurrent.scale ={ diamiter, capusleHeight, diamiter};
}

void Collider::UpdateAABBBox()
{
	// Calculate the center and half sizes of the box collider
	const Vector3 center = m_worldTransformCurrent.position;
	const Vector3 scale = m_worldTransformCurrent.scale;
	const float halfX = scale.x * 0.5f;
	const float halfY = scale.y * 0.5f;
	const float halfZ = scale.z * 0.5f;

	// Calculate the rotation matrix from the current world rotation
	Matrix4x4 R = Matrix4x4::CreateFromQuaternion(m_worldTransformCurrent.rotation);
	Vector4 xAxis = R.GetCol(0);
	Vector4 yAxis = R.GetCol(1);
	Vector4 zAxis = R.GetCol(2);

	// Calculate half sizes of the AABB
	float aabbHalfX =
		fabsf(xAxis.x) * halfX +
		fabsf(yAxis.x) * halfY +
		fabsf(zAxis.x) * halfZ;
	float aabbHalfY =
		fabsf(xAxis.y) * halfX +
		fabsf(yAxis.y) * halfY +
		fabsf(zAxis.y) * halfZ;
	float aabbHalfZ =
		fabsf(xAxis.z) * halfX +
		fabsf(yAxis.z) * halfY +
		fabsf(zAxis.z) * halfZ;

	// Update the current AABB min and max points
	m_currentAABB.min = {
		center.x - aabbHalfX,
		center.y - aabbHalfY,
		center.z - aabbHalfZ
	};
	m_currentAABB.max = {
		center.x + aabbHalfX,
		center.y + aabbHalfY,
		center.z + aabbHalfZ
	};
}

void Collider::UpdateAABBSphere()
{
	const float radius = m_currentSphereCollider.radius;
	const Vector3 center = m_worldTransformCurrent.position;

	m_currentAABB.min = {
	center.x - radius,
	center.y - radius,
	center.z - radius
	};

	m_currentAABB.max = {
	center.x + radius,
	center.y + radius,
	center.z + radius
	};
}

void Collider::UpdateAABBCapsule()
{
	const float radius = m_currentCapsuleCollider.radius;
	const Vector3 pointA = m_currentCapsuleCollider.pointA;
	const Vector3 pointB = m_currentCapsuleCollider.pointB;
	m_currentAABB.min = {
		(std::min)(pointA.x, pointB.x) - radius,
		(std::min)(pointA.y, pointB.y) - radius,
		(std::min)(pointA.z, pointB.z) - radius
	};
	m_currentAABB.max = {
		(std::max)(pointA.x, pointB.x) + radius,
		(std::max)(pointA.y, pointB.y) + radius,
		(std::max)(pointA.z, pointB.z) + radius
	};
}

void Collider::MakeSweptAABB()
{
	AABB s{};
	s.min.x = (std::min)(m_previousAABB.min.x, m_currentAABB.min.x);
	s.min.y = (std::min)(m_previousAABB.min.y, m_currentAABB.min.y);
	s.min.z = (std::min)(m_previousAABB.min.z, m_currentAABB.min.z);

	s.max.x = (std::max)(m_previousAABB.max.x, m_currentAABB.max.x);
	s.max.y = (std::max)(m_previousAABB.max.y, m_currentAABB.max.y);
	s.max.z = (std::max)(m_previousAABB.max.z, m_currentAABB.max.z);

	m_sweptAABB = s;
}

bool Collider::Serialize(nlohmann::json& outJson) const
{
	if (!Component::Serialize(outJson)) return false;

	outJson["center"] = JsonMath::ToJson(m_localTransform.position);
	outJson["rotation"] = JsonMath::ToJson(m_localTransform.rotation);
	outJson["scale"] =JsonMath::ToJson(m_localTransform.scale);

	outJson["type"] = static_cast<int>(m_type);
	outJson["layer"] = static_cast<int>(m_layer);
	outJson["isTrigger"] = m_isTrigger;

	return true;
}

bool Collider::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object()) return false;

	// Check for required fields
	if (!json.contains("name")		||
		!json.contains("center")	||
		!json.contains("rotation")	||
		!json.contains("scale")		||
		!json.contains("type")		||
		!json.contains("layer")		||
		!json.contains("isTrigger"))
	{
		return false;
	}

	// Validate field types
	if (!json["name"].is_string()			||
		!json["type"].is_number_integer()	||
		!json["layer"].is_number_integer()	||
		!json["isTrigger"].is_boolean())
	{
		return false;
	}

	ParamDesc parsedDesc;

	int parsedType = 0;
	int parsedLayer = 0;

	// Attempt to parse the fields and handle any exceptions
	try
	{
		parsedDesc.name = json["name"].get<std::string>();
		parsedType = json["type"].get<int>();
		parsedLayer = json["layer"].get<int>();
		parsedDesc.isTrigger = json["isTrigger"].get<bool>();
	}
	catch (const nlohmann::json::exception&)
	{
		return false;
	}

	// Validate parsed type
	if (!JsonMath::TryRead(json["center"], parsedDesc.localCenter)		||
		!JsonMath::TryRead(json["rotation"],parsedDesc.localRotation)	||
		!JsonMath::TryRead(json["scale"],parsedDesc.localScale))
	{
		return false;
	}

	// Validate enum range for ColliderType
	constexpr int kColliderTypeMin = static_cast<int>(ColliderType::BOX);
	constexpr int kColliderTypeMax = static_cast<int>(ColliderType::None);

	if (parsedType < kColliderTypeMin ||
		parsedType > kColliderTypeMax)
	{
		return false;
	}

	// Validate enum range for CollisionLayer
	constexpr int kCollisionLayerMin = static_cast<int>(CollisionLayer::Default);
	constexpr int kCollisionLayerMax = static_cast<int>(CollisionLayer::MAX_LAYER);

	if (parsedLayer < kCollisionLayerMin ||
		parsedLayer >= kCollisionLayerMax)
	{
		return false;
	}

	// Validate vector finiteness for center and scale
	const auto isFiniteVector3 =
		[](const Vector3& value)
		{
			return std::isfinite(value.x) &&
				std::isfinite(value.y) &&
				std::isfinite(value.z);
		};

	if (!isFiniteVector3(parsedDesc.localCenter) ||
		!isFiniteVector3(parsedDesc.localScale))
	{
		return false;
	}

	if (parsedDesc.localScale.x <= 0.0f ||
		parsedDesc.localScale.y <= 0.0f ||
		parsedDesc.localScale.z <= 0.0f)
	{
		return false;
	}

	// Validate quaternion finiteness and non-zero length for rotation
	const float rotationLengthSq = parsedDesc.localRotation.LengthSq();

	if (!std::isfinite(rotationLengthSq) ||
		rotationLengthSq <= 0.000001f)
	{
		return false;
	}

	// Set the parsed values to the output parameter
	parsedDesc.localRotation = parsedDesc.localRotation.Normalized();
	parsedDesc.type = static_cast<ColliderType>(parsedType);
	parsedDesc.layer = static_cast<CollisionLayer>(parsedLayer);

	SetParams(parsedDesc);

	// Reset old member variables to default values
	m_collisionInfos.clear();
	m_isDetected = false;
	m_deleteFlag = false;
	m_isActive = true;
	m_transformGeneration = static_cast<uint64_t>(-1);
	m_isDirty = true;

	return true;
}
