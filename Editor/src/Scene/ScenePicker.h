#pragma once
#include <optional>
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Component/RendererComponent.h"

class SceneBase;
class MeshRenderer;
class SpriteRenderer;
class UIRenderer;
struct CameraInfo;

//---------------------------------------------------------------------
// ScenePicker class
// Provides functionality to pick actors in the scene using raycasting
// Buld ray based on camera information and viewport coordinates,
// and check for intersections with actors' meshes or sprites
//---------------------------------------------------------------------

// Struct to hold information about a ray in the scene
struct SceneRay
{
	Vector3 origin;
	Vector3 direction;
};

// Struct to hold information of the Actor hit by a ray
struct ScenePickHit
{
	Guid actorGuid;
	float distance;
};

class ScenePicker
{
public:
	// Pick an actor in the scene based on the camera information and viewport coordinates
	// Launch a ray from the point in the viewport and check for intersection with actors' meshes or sprites
	// Returns the nearest hit information of the actor hit by the ray, or std::nullopt if no actor was hit
	static std::optional<ScenePickHit> Pick(
		SceneBase& scene,				// Scene to pick from (to get actors)
		const CameraInfo& cameraInfo,	// Camera information for building the ray
		const Vector2& viewportUV,		// Position in the viewport where the ray is launched (normalized coordinates [0,1])
		RenderSpace targetRenderSpace	// Target render space to filter the pick (World or Screen space)
	);

private:
	// Build a ray from the camera's position and direction 
	// based on the viewport coordinates
	static SceneRay BuildRay(
		const CameraInfo& cameraInfo,
		const Vector2& viewportUV
	);

	// Check for intersection between a ray and a 
	// sphere defined by its center and radius
	// Used by MeshRenderer for bounding sphere intersection test
	static bool IntersectSphere(
		const SceneRay& ray,
		const Vector3& center,
		float radius,
		float& outDistance
	);

	// Check for intersection between a ray and a MeshRenderer's bounding sphere
	static bool IntersectMesh(
		const SceneRay& ray,
		class MeshRenderer& renderer,
		float& outDistance
	);

	// Check for intersection between a ray and a SpriteRenderer's bounding box
	static bool IntersectSprite(
		const SceneRay& ray,
		const CameraInfo& cameraInfo,
		class SpriteRenderer& renderer,
		float& outDistance
	);

	// Check for intersection between a ray and a UIRenderer's quad
	static bool IntersectUI(
		const SceneRay& ray,
		UIRenderer& renderer,
		float& outDistance
	);

	// Check for intersection between a ray and a quad defined in local space
	static bool IntersectLocalQuad(
		const SceneRay& ray,
		const Matrix4x4& worldMatrix,
		float minX,
		float maxX,
		float minY,
		float maxY,
		float& outDistance
	);
};