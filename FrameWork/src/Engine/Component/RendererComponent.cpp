#include "RendererComponent.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"

void RendererComponent::CheckIfTransformChanged()
{
	auto owner = GetOwner();
	if (owner) {
		auto transform = owner->GetComponentByClass<Transform>();
		if (transform) {
			uint64_t currentGeneration = transform->GetWorldGeneration();
			if (m_transformGeneration != currentGeneration) {
				m_transformGeneration = currentGeneration;
				m_isProxyDirty = true;
			}
		}
	}
}