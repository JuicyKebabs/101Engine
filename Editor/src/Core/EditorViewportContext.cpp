#include "Core/EditorViewportContext.h"

#include "Engine/Scene/SceneBase.h"

void EditorViewportContext::Revalidate(SceneBase* scene)
{
	if (m_canvasEditContext.HasTarget() &&
		(!scene || !m_canvasEditContext.ResolveCanvas(*scene)))
	{
		m_canvasEditContext.Clear();
		m_canvasNavigation = {};
	}
}

void EditorViewportContext::ResetSceneTargets()
{
	m_canvasEditContext.Clear();
	m_canvasNavigation = {};
}
