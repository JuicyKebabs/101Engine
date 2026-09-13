#pragma once

#include <cstdint>

#include "Core/CanvasEditContext.h"
#include "Core/EditorViewCamera.h"
#include "Engine/Core/Guid/Guid.h"
#include "Engine/Core/Math/Math.h"

enum class EditorViewportMode
{
	Scene,
	Canvas,
};

struct CanvasViewNavigation
{
	Vector2 center = Vector2::Zero();
	float zoom = 1.0f;
	Guid canvasActorGuid;
};

// Persistent view state for one Editor Document. GPU render targets and the
// SceneViewPanel remain shared by the Editor window.
class EditorViewportContext
{
public:
	void Initialize(uint32_t width, uint32_t height)
	{
		m_sceneCamera.Initialize(width, height);
	}

	EditorViewportMode GetViewMode() const { return m_viewMode; }
	void SetViewMode(EditorViewportMode mode) { m_viewMode = mode; }

	EditorViewCamera& GetSceneCamera() { return m_sceneCamera; }
	const EditorViewCamera& GetSceneCamera() const { return m_sceneCamera; }
	CanvasEditContext& GetCanvasEditContext() { return m_canvasEditContext; }
	const CanvasEditContext& GetCanvasEditContext() const { return m_canvasEditContext; }
	CanvasViewNavigation& GetCanvasNavigation() { return m_canvasNavigation; }
	const CanvasViewNavigation& GetCanvasNavigation() const { return m_canvasNavigation; }

	void Revalidate(SceneBase* scene);
	void ResetSceneTargets();

private:
	EditorViewportMode m_viewMode = EditorViewportMode::Scene;
	EditorViewCamera m_sceneCamera;
	CanvasEditContext m_canvasEditContext;
	CanvasViewNavigation m_canvasNavigation;
};
