#include "ScenePicker.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/UI/UIRenderer.h"

#include <algorithm>
#include <cmath>
#include <limits>

std::optional<ScenePickHit> ScenePicker::Pick(
	SceneBase& scene,
	const CameraInfo& cameraInfo,
	const Vector2& viewportUV,
	RenderSpace targetRenderSpace
)
{
	// Validate the viewport coordinates 
	// to ensure they are within the normalized range [0, 1]
	if (viewportUV.x < 0.0f ||
		viewportUV.x > 1.0f ||
		viewportUV.y < 0.0f ||
		viewportUV.y > 1.0f)
	{
		return std::nullopt;
	}
 
	// Build ray from the camera information and the point in the viewport (normalized coordinates)
	const SceneRay ray = BuildRay(cameraInfo, viewportUV);

	std::optional<ScenePickHit> nearestHit;	// Store the nearest hit information (actor GUID and distance)
	uint64_t nearestScreenSortKey = 0;		// Store the sort key for the nearest hit in screen space (canvas order and sort order)

	// Iterate through all actors in the scene to check for intersection with the ray
	for (Actor* actor : scene.GetAllActors())
	{
		if (!actor || !actor->IsActive() || actor->IsDestroyed()) continue;

		float distance = 0.0f;		// Distance from the ray origin to the intersection point
		uint64_t screenSortKey = 0;	// Sort key for screen space rendering (canvas order and sort order)
		bool hit = false;			// Flag to indicate if the ray hit the actor's renderer

		// Check for intersection with MeshRenderer or SpriteRenderer components
		if (MeshRenderer* meshRenderer = actor->GetComponentByClass<MeshRenderer>())
		{
			// Check if the render space matches the target render space
			if (meshRenderer->GetRenderSpace() != targetRenderSpace) continue;

			hit = IntersectMesh(ray, *meshRenderer, distance);

			if (hit)
			{
				const MeshRendererProxy& proxy = meshRenderer->GetRenderProxy();
				screenSortKey = (static_cast<uint64_t>(proxy.common.canvasOrder) << 32) | static_cast<uint64_t>(proxy.common.sortOrder);
			}
		}
		else if (SpriteRenderer* spriteRenderer = actor->GetComponentByClass<SpriteRenderer>())
		{
			if (spriteRenderer->GetRenderSpace() != targetRenderSpace) continue;

			hit = IntersectSprite(ray, cameraInfo, *spriteRenderer, distance);

			if (hit)
			{
				const SpriteRendererProxy& proxy = spriteRenderer->GetRenderProxy(cameraInfo);
				screenSortKey = (static_cast<uint64_t>(proxy.common.canvasOrder) << 32) | static_cast<uint64_t>(proxy.common.sortOrder);
			}
		}
		else if (UIRenderer* uiRenderer = actor->GetComponentByClass<UIRenderer>())
		{
			if (uiRenderer->GetRenderSpace() != targetRenderSpace) continue;

			hit = IntersectUI(ray, *uiRenderer, distance);

			if (hit)
			{
				const UIRendererProxy& proxy = uiRenderer->GetRenderProxy();
				screenSortKey = (static_cast<uint64_t>(proxy.common.canvasOrder) << 32) | static_cast<uint64_t>(proxy.common.sortOrder);
			}
		}

		if (!hit) continue;

		// Compare the distance of the current hit with the nearest hit found so far

		if (targetRenderSpace == RenderSpace::Screen)
		{
			// In case of screen space , check the order of the elements in addition to the distance, 
			//to ensure that the topmost element is selected when multiple elements are at the same distance

			const float distanceEpsilon = 1e-6f; // A small threshold to avoid floating-point precision issues
			
			// Check if the current hit has a higher order than the stored nearest hit 
			// or if nearest hit is not set yet (first hit)
			const bool hasHigherOrder = !nearestHit || screenSortKey > nearestScreenSortKey;

			// Check if the current hit has the same order as the nearest hit
			const bool hasSameOrder = nearestHit && screenSortKey == nearestScreenSortKey;

			// Check if the current hit is closer than the nearest hit when they have the same order
			const bool isCloserAtSameOrder = hasSameOrder && distance < nearestHit->distance - distanceEpsilon;

			// Check if the nearest hit should be updated based on the order and distance comparison
			if (hasHigherOrder || isCloserAtSameOrder)
			{
				nearestHit = ScenePickHit{ actor->GetGuid(), distance };

				nearestScreenSortKey = screenSortKey;
			}
		}
		else
		{
			// For world space, simply check if this hit is 
			// closer than the nearest hit found so far
			if (!nearestHit || distance < nearestHit->distance)
			{
				nearestHit = ScenePickHit{ actor->GetGuid(), distance };
			}
		}
	}

	return nearestHit;
}

SceneRay ScenePicker::BuildRay(
	const CameraInfo& cameraInfo,
	const Vector2& viewportUV
)
{
	// Calculate the near point in world space 
	// by unprojecting the viewport coordinates at the near plane (z = 0.0)
	const DirectX::XMVECTOR nearPoint =
		DirectX::XMVector3Unproject(
			DirectX::XMVectorSet(viewportUV.x, viewportUV.y, 0.0f, 1.0f),
			0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
			cameraInfo.projMatrix,
			cameraInfo.viewMatrix,
			DirectX::XMMatrixIdentity()
		);

	// Calculate the far point in world space 
	// by unprojecting the viewport coordinates at the far plane (z = 1.0)
	const DirectX::XMVECTOR farPoint =
		DirectX::XMVector3Unproject(
			DirectX::XMVectorSet(viewportUV.x, viewportUV.y, 1.0f, 1.0f),
			0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
			cameraInfo.projMatrix,
			cameraInfo.viewMatrix,
			DirectX::XMMatrixIdentity()
		);

	// Convert the DirectX::XMVECTOR points to Vector3
	const Vector3 origin(nearPoint);
	const Vector3 farPosition(farPoint);

	// Build the ray with the origin and normalized direction
	SceneRay ray;
	ray.origin = origin;
	ray.direction = (farPosition - origin).Normalized();

	return ray;
}

bool ScenePicker::IntersectSphere(
	const SceneRay& ray,
	const Vector3& center,
	float radius,
	float& outDistance
)
{
	if (radius <= 0.0f) return false;

	const Vector3 originToCenter = ray.origin - center;

	const float b = originToCenter.Dot(ray.direction);

	const float c = originToCenter.LengthSq() - radius * radius;

	const float discriminant = b * b - c;

	if (discriminant < 0.0f) return false;

	const float offset = std::sqrt(discriminant);

	float distance = -b - offset;

	if (distance < 0.0f) distance = -b + offset;

	if (distance < 0.0f) return false;

	outDistance = distance;

	return true;
}

bool ScenePicker::IntersectMesh(
	const SceneRay& ray,
	MeshRenderer& renderer,
	float& outDistance
)
{
	// Validate the MeshRenderer for intersection testing
	if (!renderer.IsVisible() || !renderer.IsConfigured())
	{
		return false;
	}

	// Get the render proxy which has the dynamic mesh data built in every frame
	const MeshRendererProxy& proxy = renderer.GetRenderProxy();

	Vector3 position;
	Quaternion rotation;
	Vector3 scale;

	// Decompose the world matrix of the proxy to get position, rotation, and scale
	if (!proxy.common.worldMatrix.Decompose(position, rotation, scale))
	{
		return false;
	}

	constexpr float epsilon = 1e-6f; // A small threshold to avoid division by zero or degenerate scale

	// Check if any scale component is too close to zero, 
	// which would make the mesh degenerate
	if (std::abs(scale.x) < epsilon ||
		std::abs(scale.y) < epsilon ||
		std::abs(scale.z) < epsilon)
	{
		return false;
	}

	// Get max scale to create sphere which covers the whole mesh
	const float maxScale = std::max({
		std::abs(scale.x),
		std::abs(scale.y),
		std::abs(scale.z)
		});

	bool hit = false;

	// Buffer to track the nearest intersection distance
	float nearestDistance = std::numeric_limits<float>::max();

	for (const SubmeshRenderTemplate& renderTemplate : renderer.GetRenderTemplates())
	{
		const MeshDesc& meshDesc = renderTemplate.meshDesc;

		// Transform the mesh's bounding center from local space to world space
		const Vector3 worldCenter = proxy.common.worldMatrix.TransformPoint(meshDesc.boundsCenter);

		// Scale the mesh's bounding radius by the maximum scale to get the world radius
		const float worldRadius = meshDesc.boundsRadius * maxScale;

		float distance = 0.0f;

		// Check if the ray intersects the bounding sphere of the mesh
		if (!IntersectSphere(ray, worldCenter, worldRadius, distance)) continue;

		hit = true;

		// Compoare the intersection distance with the nearest distance found so far
		nearestDistance = std::min(nearestDistance, distance);
	}

	if (!hit) return false;

	outDistance = nearestDistance;
	return true;
}

bool ScenePicker::IntersectSprite(
	const SceneRay& ray,
	const CameraInfo& cameraInfo,
	SpriteRenderer& renderer,
	float& outDistance
)
{
	// Validate the SpriteRenderer for intersection testing
	if (!renderer.IsVisible() || !renderer.IsConfigured())
	{
		return false;
	}

	// Get the render proxy which has the dynamic sprite data built in every frame
	const SpriteRendererProxy& proxy = renderer.GetRenderProxy(cameraInfo);

	// Calculate the bounds of the sprite in local space considering pivot and flip
	float x0 = -proxy.pivot.x * proxy.flip.x;
	float x1 = (1.0f - proxy.pivot.x) * proxy.flip.x;
	float y0 = -proxy.pivot.y * proxy.flip.y;
	float y1 = (1.0f - proxy.pivot.y) * proxy.flip.y;

	// Check for intersection between the ray and the sprite's quad in local space
	return IntersectLocalQuad(
		ray,
		proxy.common.worldMatrix,
		std::min(x0, x1),
		std::max(x0, x1),
		std::min(y0, y1),
		std::max(y0, y1),
		outDistance
	);
}

bool ScenePicker::IntersectUI(
	const SceneRay& ray,
	UIRenderer& renderer,
	float& outDistance
)
{
	if (!renderer.IsVisible() || !renderer.IsConfigured())
	{
		return false;
	}

	const UIRendererProxy& proxy = renderer.GetRenderProxy();

	return IntersectLocalQuad(
		ray,
		proxy.common.worldMatrix,
		-0.5f,
		0.5f,
		-0.5f,
		0.5f,
		outDistance
	);
}

bool ScenePicker::IntersectLocalQuad(
	const SceneRay& ray,
	const Matrix4x4& worldMatrix,
	float minX,
	float maxX,
	float minY,
	float maxY,
	float& outDistance
)
{
	const Matrix4x4 inverseWorld = worldMatrix.Inverse();

	// Transform the ray back to the local space of the quad using the inverse world matrix
	const Vector3 localOrigin = inverseWorld.TransformPoint(ray.origin);
	const Vector3 localDirection = inverseWorld.TransformDirection(ray.direction);

	constexpr float epsilon = 1e-6f; // A small threshold to avoid division by zero

	if (std::abs(localDirection.z) < epsilon)
	{
		return false; // Ray is parallel to the quad's plane
	}

	// Calculate the distance along the ray to the intersection point with the quad's plane 
	// (z = 0 in local space)
	const float localDistance = -localOrigin.z / localDirection.z;

	if (localDistance < 0.0f) return false; // Intersection is behind the ray origin

	// Calculate the intersection point in local space
	const Vector3 localHit = localOrigin + localDirection * localDistance;

	// Check if the intersection point is within the bounds of the quad
	if (localHit.x < minX ||
		localHit.x > maxX ||
		localHit.y < minY ||
		localHit.y > maxY)
	{
		return false;
	}

	// Convert the local hit point back to world space
	const Vector3 worldHit = worldMatrix.TransformPoint(localHit);

	const float worldDistance = (worldHit - ray.origin).Dot(ray.direction);

	if (worldDistance < 0.0f) return false; // Intersection is behind the ray origin

	outDistance = worldDistance;

	return true;
}