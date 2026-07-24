#pragma once
#include <cstddef>
#include <memory>
#include <vector>
#include "IEditorCommand.h"

//-------------------------------------------------
// EditorCommandHistory class
// Manages the history of executed editor commands,
// allowing for undo and redo operations.
//-------------------------------------------------

class EditorCommandHistory
{
public:
	bool Execute(std::unique_ptr<IEditorCommand> command);
	bool Undo();
	bool Redo();

	void Clear();

	bool CanUndo() const { return !m_undoStack.empty(); }
	bool CanRedo() const { return !m_redoStack.empty(); }

	std::size_t GetUndoCount() const { return m_undoStack.size(); }
	std::size_t GetRedoCount() const { return m_redoStack.size(); }

private:
	std::vector<std::unique_ptr<IEditorCommand>> m_undoStack;
	std::vector<std::unique_ptr<IEditorCommand>> m_redoStack;
};