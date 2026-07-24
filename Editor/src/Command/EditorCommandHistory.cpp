#include "EditorCommandHistory.h"

bool EditorCommandHistory::Execute(std::unique_ptr<IEditorCommand> command)
{
	if (!command) return false;

	// Execute the command and check if it was successful
	if (!command->Execute()) return false;

	// Move the command to the undo stack
	m_undoStack.push_back(std::move(command));

	// Clear redo stack on new command execution
	m_redoStack.clear();

	return true;
}

bool EditorCommandHistory::Undo()
{
	if (m_undoStack.empty()) return false;

	// Get the last command from the undo stack before moving it to the redo stack
	// to ensure we can call Undo() on it before it's moved.
	IEditorCommand* command = m_undoStack.back().get();

	// Call Undo() on the command before moving it to the redo stack
	if (!command->Undo()) return false;

	// Move the command to the redo stack
	m_redoStack.push_back(std::move(m_undoStack.back()));

	// Remove the command from the undo stack
	m_undoStack.pop_back();

	return true;
}

bool EditorCommandHistory::Redo()
{
	if (m_redoStack.empty()) return false;

	// Get the last command from the redo stack before moving it to the undo stack
	// to ensure we can call Execute() on it before it's moved.
	IEditorCommand* command = m_redoStack.back().get();

	// Call Execute() on the command before moving it to the undo stack
	if (!command->Execute()) return false;

	// Move the command back to the undo stack
	m_undoStack.push_back(std::move(m_redoStack.back()));

	// Remove the command from the redo stack
	m_redoStack.pop_back();
	
	return true;
}

void EditorCommandHistory::Clear()
{
	m_undoStack.clear();
	m_redoStack.clear();
}