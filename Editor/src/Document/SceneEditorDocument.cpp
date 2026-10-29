#include "Document/SceneEditorDocument.h"

#include <filesystem>

#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/SceneWriter.h"

SceneEditorDocument::SceneEditorDocument(
	std::unique_ptr<SceneBase> scene,
	std::string filePath,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	Guid sourceAssetGuid)
	: IEditorDocument(viewportWidth, viewportHeight)
	, m_scene(std::move(scene))
	, m_filePath(std::move(filePath))
	, m_sourceAssetGuid(sourceAssetGuid)
{
	// A loaded Scene has attached Components but may still have cold World
	// caches. Document creation can happen inside RenderImGui, after the normal
	// EditorUpdate. Synchronize the new hierarchy before exposing it to drawing.
	// EditorUpdate only flushes Transform/Collider/Camera and deferred deletion;
	// it does not Start or advance gameplay. Ordinary per-Frame edit timing stays
	// in EditorApp::Update and is deliberately not moved here.
	if (m_scene)
	{
		m_scene->EditorUpdate(0.0f);
	}

	UpdateDisplayName();
}

SceneEditorDocument::~SceneEditorDocument()
{
	ReleaseScene();
}

bool SceneEditorDocument::Save()
{
	if (!m_scene || m_filePath.empty())
	{
		return false;
	}

	if (!SceneWriter::SaveScene(m_filePath, m_scene.get()))
	{
		return false;
	}

	MarkClean();
	return true;
}

void SceneEditorDocument::ReplaceScene(
	std::unique_ptr<SceneBase> scene,
	std::string filePath,
	bool markDirty,
	Guid sourceAssetGuid)
{
	// Complete the incoming Scene's initial drawing state while the previous
	// document is still intact. An exception during this preparation leaves its
	// Scene, history and selection owned by this document. The caller supplies a
	// published Scene (the normal SceneLoader::LoadCandidate path), not a deferred
	// unpublished candidate, whose EditorUpdate intentionally does nothing.
	if (scene)
	{
		scene->EditorUpdate(0.0f);
	}

	ClearCommandHistory();
	GetSelection().Clear();
	GetViewportContext().ResetSceneTargets();

	auto previous = std::move(m_scene);
	m_scene = std::move(scene);
	m_filePath = std::move(filePath);
	m_sourceAssetGuid = sourceAssetGuid;
	UpdateDisplayName();

	if (markDirty)
	{
		MarkDirty();
	}
	else
	{
		MarkClean();
	}

	if (previous)
	{
		previous->Finalize();
	}
}

bool SceneEditorDocument::UpdateAssetPath(const Guid& expectedGuid, std::string filePath)
{
	if (!expectedGuid.IsValid() || m_sourceAssetGuid != expectedGuid || filePath.empty())
	{
		return false;
	}

	m_filePath = std::move(filePath);
	UpdateDisplayName();
	return true;
}

void SceneEditorDocument::ReleaseScene()
{
	ClearCommandHistory();
	GetSelection().Clear();
	GetViewportContext().ResetSceneTargets();

	if (m_scene)
	{
		m_scene->Finalize();
	}

	m_scene.reset();
}

void SceneEditorDocument::UpdateDisplayName()
{
	if (m_filePath.empty())
	{
		m_displayName = "Untitled Scene";
		return;
	}

	m_displayName = std::filesystem::path(m_filePath).filename().string();

	if (m_displayName.empty())
	{
		m_displayName = m_filePath;
	}
}
