#include "Engine/Component/TransformConversion.h"

TransformKind TransformConversion::GetKind(const Transform& transform)
{
	if (dynamic_cast<const RectTransform*>(&transform))
	{
		return TransformKind::RectTransform;
	}

	return TransformKind::Transform;
}

TransformSnapshot TransformConversion::Capture(const Transform& transform)
{
	// Capture the common state of the Transform-family component
	TransformSnapshot snapshot;

	snapshot.sourceKind = GetKind(transform);
	snapshot.localTransform = transform.GetLocalTransform();
	snapshot.effectiveLocalTransform = transform.GetLocalTransform();
	snapshot.name = transform.GetName();

	// If the transform is a RectTransform, capture its specific state
	auto* rectTransform = dynamic_cast<const RectTransform*>(&transform);

	if (rectTransform)
	{
		// Capture the RectTransform-specific state
		RectTransformState state;
		state.anchorMode = rectTransform->GetAnchorMode();
		state.anchoredPosition = rectTransform->GetAnchoredPosition();
		state.pivot = rectTransform->GetPivot();
		state.size = rectTransform->GetSize();
		snapshot.rectTransformState = state;

		// Capture the actual local pose after applying Anchor, Anchored Position,
		// and Pivot offsets. This is used when converting to a normal Transform.
		Transform3D effectiveLocal;
		rectTransform->GetLocalMatrix().Decompose(
			effectiveLocal.position,
			effectiveLocal.rotation,
			effectiveLocal.scale
		);
		snapshot.effectiveLocalTransform = effectiveLocal;
	}

	return snapshot;
}

std::unique_ptr<Transform> TransformConversion::Create(
	TransformKind targetKind,
	const TransformSnapshot& snapshot
)
{
	switch (targetKind)
	{
	case TransformKind::Transform:
	{// Convert to a normal Transform
		auto transform = std::make_unique<Transform>();

		// Use the effective local pose so RectTransform layout offsets are preserved
		Transform::ParamDesc desc;
		desc.localPosition = snapshot.effectiveLocalTransform.position;
		desc.localRotation = snapshot.effectiveLocalTransform.rotation;
		desc.localScale = snapshot.effectiveLocalTransform.scale;
		desc.name = snapshot.name;

		transform->SetParams(desc);

		return transform;
	}

	case TransformKind::RectTransform:
	{// Convert to a RectTransform
		auto rectTransform = std::make_unique<RectTransform>();

		Transform::ParamDesc transformDesc;
		RectTransform::ParamDesc rectDesc;

		if (snapshot.rectTransformState)
		{// In case of specific params of RectTransform are available
			// Restore all RectTransform-specific state when the source was a RectTransform
			transformDesc.localPosition = snapshot.localTransform.position;
			transformDesc.localRotation = snapshot.localTransform.rotation;
			transformDesc.localScale = snapshot.localTransform.scale;

			rectDesc.anchorMode = snapshot.rectTransformState->anchorMode;
			rectDesc.anchoredPosition = snapshot.rectTransformState->anchoredPosition;
			rectDesc.pivot = snapshot.rectTransformState->pivot;
			rectDesc.size = snapshot.rectTransformState->size;
		}
		else
		{// In case of no specific params of RectTransform are available (source was a normal Transform)
			// X/Y becomes anchored position; Z remains RectTransform depth
			transformDesc.localPosition = Vector3(0.0f, 0.0f, snapshot.localTransform.position.z);
			transformDesc.localRotation = snapshot.localTransform.rotation;
			transformDesc.localScale = snapshot.localTransform.scale;

			// Set default values for RectTransform
			rectDesc.anchorMode = AnchorMode::MiddleCenter;
			rectDesc.anchoredPosition = Vector2(
				snapshot.localTransform.position.x,
				snapshot.localTransform.position.y
			);
			rectDesc.pivot = Vector2(0.5f, 0.5f);
			rectDesc.size = Vector2(100.0f, 100.0f);
		}

		transformDesc.name = snapshot.name;
		rectDesc.name = snapshot.name;

		static_cast<Transform&>(*rectTransform).SetParams(transformDesc);
		rectTransform->SetParams(rectDesc);

		return rectTransform;
	}

	default:
		return nullptr;
	}
}
