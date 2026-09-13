#include "Document/IEditorDocument.h"

bool IEditorDocument::ExecuteCommand(std::unique_ptr<IEditorCommand> command)
{
	if (!m_commandHistory.Execute(std::move(command))) return false;
	m_dirty = true;
	return true;
}

bool IEditorDocument::RecordExecutedCommand(std::unique_ptr<IEditorCommand> command)
{
	if (!m_commandHistory.RecordExecuted(std::move(command))) return false;
	m_dirty = true;
	return true;
}

bool IEditorDocument::Undo()
{
	if (!m_commandHistory.Undo()) return false;
	m_dirty = true;
	return true;
}

bool IEditorDocument::Redo()
{
	if (!m_commandHistory.Redo()) return false;
	m_dirty = true;
	return true;
}
