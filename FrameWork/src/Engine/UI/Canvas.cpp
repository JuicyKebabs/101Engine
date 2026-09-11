#include "Canvas.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Core/Serialization/JsonMath.h"
#include <cmath>
#include <limits>
#include <algorithm>

Vector2 Canvas::GetLayoutReferenceSize() const
{
	Actor* owner = GetOwner();
	if (!owner) return m_referenceSize;

	// World-Space Canvas always uses its authored logical size.
	if (m_effectiveRenderMode == CanvasRenderMode::WorldSpace)
	{
		return m_referenceSize;
	}

	// Scale-With-Screen-Size lays out children in the Canvas's authored
	// logical resolution. GetScaleFactor maps this logical space to the
	// actual display area (viewport for a root Canvas, RectTransform size
	// for a nested Canvas).
	if (m_scaleMode == CanvasScaleMode::ScaleWithScreenSize)
	{
		return m_referenceSize;
	}

	// Constant-Pixel-Size uses the actual display area as its layout space,
	// keeping one layout unit equal to one parent-space pixel unit.
	if (!IsRootCanvas())
	{
		RectTransform* rectTransform =
			owner->GetComponentByClass<RectTransform>();

		return rectTransform
			? rectTransform->GetSize()
			: Vector2::One();
	}

	SceneBase* scene = owner->GetOwner();

	return scene
		? scene->GetViewportSize()
		: Vector2::One();
}

bool Canvas::IsRootCanvas() const
{
	Actor* owner = GetOwner();
	if (!owner) return false;

	// Traverse the hierarchy of parent actors to check if any ancestor has a Canvas component
	for (Actor* ancestor = owner->GetParent(); ancestor; ancestor = ancestor->GetParent())
	{
		if (ancestor->GetComponentByClass<Canvas>())
		{
			return false;
		}
	}

	return true;
}

bool Canvas::IsHierarchyVisible() const
{
	// Traverse the hierarchy of parent actors to check if any ancestor Canvas is not visible
	for (Actor* actor = GetOwner(); actor; actor = actor->GetParent())
	{
		Canvas* canvas = actor->GetComponentByClass<Canvas>();
		if (canvas && !canvas->IsVisible()) return false;
	}

	return true;
}

Matrix4x4 Canvas::GetContentWorldMatrix() const
{
	Actor* owner = GetOwner();
	if (!owner) return Matrix4x4::Identity();

	Transform* transform = owner->GetComponentByClass<Transform>();

	return transform
		? transform->GetWorldTransform().GetMatrix()
		: Matrix4x4::Identity();
}

bool Canvas::ContainsCanvas(const Canvas* canvas) const
{
	if (!canvas) return false;

	Actor* rootActor = GetOwner();
	Actor* current = canvas->GetOwner();

	if (!rootActor || !current) return false;

	for (; current; current = current->GetParent())
	{
		if (current == rootActor) return true;
	}

	return false;
}

bool Canvas::ContainsRenderer(const RendererComponent* renderer) const
{
	return renderer && ContainsCanvas(renderer->GetGoverningCanvas());
}

float Canvas::GetScaleFactor() const
{
	if (m_effectiveRenderMode != CanvasRenderMode::ScreenSpace) return 1.0f;
	if (m_scaleMode == CanvasScaleMode::ConstantPixelSize) return 1.0f;

	Actor* owner = GetOwner();
	if (!owner) return 1.0f;

	Vector2 displaySize = Vector2::One();

	if (IsRootCanvas())
	{
		SceneBase* scene = owner->GetOwner();
		if (!scene) return 1.0f;

		displaySize = scene->GetViewportSize();
	}
	else
	{
		RectTransform* rectTransform =
			owner->GetComponentByClass<RectTransform>();

		if (!rectTransform) return 1.0f;

		displaySize = rectTransform->GetSize();
	}

	if (displaySize.x <= 0.0f ||
		displaySize.y <= 0.0f ||
		m_referenceSize.x <= 0.0f ||
		m_referenceSize.y <= 0.0f)
	{
		return 1.0f;
	}

	// A nested Canvas scales its logical resolution into its RectTransform
	// display size in exactly the same way that a root Canvas scales into
	// the viewport.
	const float scaleX = displaySize.x / m_referenceSize.x;
	const float scaleY = displaySize.y / m_referenceSize.y;

	// Clamp the matchWidthOrHeight value to the range [0, 1]
	const float match = std::clamp(m_matchWidthOrHeight, 0.0f, 1.0f);

	// Calculate the logarithmic scale factor using the match value to
	// interpolate between width and height scales
	const float logWidth = std::log2(scaleX);
	const float logHeight = std::log2(scaleY);
	const float logScale = logWidth + (logHeight - logWidth) * match;

	// Return the final scale factor by exponentiating the logarithmic scale
	return std::pow(2.0f, logScale);
}
