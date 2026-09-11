#include <cmath>
#include <algorithm>
#include "RendererComponent.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Core/Serialization/JsonMath.h"

std::vector<uint32_t> RendererComponent::BuildCanvasSortPath() const
{
	std::vector<uint32_t> sortPath;

	Canvas* canvas = GetGoverningCanvas();
	if (!canvas) return sortPath; // No governing canvas, return empty path

	// Traverse up the actor hierarchy to collect sort orders from all ancestor canvases
	for (Actor* current = canvas->GetOwner(); current != nullptr; current = current->GetParent())
	{
		Canvas* currentCanvas = current->GetComponentByClass<Canvas>();

		if (currentCanvas)
		{
			sortPath.push_back(currentCanvas->GetSortOrder());
		}
	}

	// Reverse the collected sort orders to have the root canvas first and the current renderer's canvas last
	std::reverse(sortPath.begin(), sortPath.end());

	// Finally, append this renderer's sort order in its governing canvas
	sortPath.push_back(m_sortOrderInCanvas);

	return sortPath;
}

bool RendererComponent::IsVisible() const
{
	if (!m_isVisible) return false;

	// If the governing canvas is set, check its visibility in the hierarchy
	return !m_pGoverningCanvas || m_pGoverningCanvas->IsHierarchyVisible();
}

RenderSpace RendererComponent::GetRenderSpace() const
{
	if (m_pGoverningCanvas) 
	{
		// If the governing canvas is set, determine the render space based on its render mode
		if (m_pGoverningCanvas->GetRenderMode() == CanvasRenderMode::ScreenSpace)
		{// If the governing canvas is in screen space, return Screen render space
			return RenderSpace::Screen;
		}
	}

	// If no governing canvas is set or 
	// if the governing canvas is in world space, 
	// return World render space
	return RenderSpace::World;
}

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
