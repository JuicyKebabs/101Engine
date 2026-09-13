#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "Command/EditorCommandHistory.h"
#include "Core/EditorSelection.h"
#include "Core/EditorViewportContext.h"
#include "Engine/Core/GUID/Guid.h"

class EditorDocumentManager;
class SceneBase;

enum class EditorDocumentType
{
	Scene,
	ActorImprint,
};

// One independently editable unit in the Editor. Common state lives here so
// every future Document type gets isolated history, selection, viewport, and
// dirty tracking without EditorApp branching on its concrete type.
class IEditorDocument
{
public:
	IEditorDocument(const IEditorDocument&) = delete;
	IEditorDocument& operator=(const IEditorDocument&) = delete;
	virtual ~IEditorDocument() = default;

	virtual SceneBase* GetWorkingScene() = 0;
	virtual const SceneBase* GetWorkingScene() const = 0;
	virtual bool Save() = 0;
	virtual bool CanEnterPlay() const = 0;
	virtual EditorDocumentType GetType() const = 0;
	virtual std::string_view GetDisplayName() const = 0;
	virtual Guid GetSourceAssetGuid() const { return {}; }

	EditorCommandHistory& GetCommandHistory() { return m_commandHistory; }
	const EditorCommandHistory& GetCommandHistory() const { return m_commandHistory; }
	EditorSelection& GetSelection() { return m_selection; }
	const EditorSelection& GetSelection() const { return m_selection; }
	EditorViewportContext& GetViewportContext() { return m_viewportContext; }
	const EditorViewportContext& GetViewportContext() const { return m_viewportContext; }

	virtual bool ExecuteCommand(std::unique_ptr<IEditorCommand> command);
	bool RecordExecutedCommand(std::unique_ptr<IEditorCommand> command);
	bool Undo();
	bool Redo();
	void ClearCommandHistory() { m_commandHistory.Clear(); }

	bool IsDirty() const { return m_dirty; }
	std::uint64_t GetDirtyGeneration() const { return m_dirtyGeneration; }
	void MarkDirty()
	{
		m_dirty = true;
		++m_dirtyGeneration;
	}
	void MarkClean() { m_dirty = false; }

protected:
	IEditorDocument(uint32_t viewportWidth, uint32_t viewportHeight)
	{
		m_viewportContext.Initialize(viewportWidth, viewportHeight);
	}

private:
	friend class EditorDocumentManager;
	virtual std::unique_ptr<SceneBase>* GetWorkingSceneOwnerSlot() = 0;

	EditorCommandHistory m_commandHistory;
	EditorSelection m_selection;
	EditorViewportContext m_viewportContext;
	bool m_dirty = false;
	std::uint64_t m_dirtyGeneration = 0;
};
