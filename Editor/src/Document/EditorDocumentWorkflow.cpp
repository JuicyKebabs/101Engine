#include "Document/EditorDocumentWorkflow.h"

bool EditorDocumentWorkflow::Activate(EditorDocumentId id)
{
	if (m_exitPending)
	{
		return false;
	}

	return m_documents.ActivateDocument(id);
}

EditorDocumentWorkflowResult EditorDocumentWorkflow::RequestClose(EditorDocumentId id)
{
	if (m_exitPending || HasPendingDecision())
	{
		return EditorDocumentWorkflowResult::NoAction;
	}

	IEditorDocument* document = m_documents.FindDocument(id);

	if (!document)
	{
		return EditorDocumentWorkflowResult::NotFound;
	}

	if (document->IsDirty())
	{
		m_pendingDocumentId = id;
		return EditorDocumentWorkflowResult::ConfirmationRequired;
	}

	if (m_documents.CloseDocument(id, EditorDocumentCloseDecision::Discard) ==
		EditorDocumentCloseResult::Closed)
	{
		return EditorDocumentWorkflowResult::Closed;
	}

	return EditorDocumentWorkflowResult::NotFound;
}

EditorDocumentWorkflowResult EditorDocumentWorkflow::RequestExit()
{
	if (m_exitPending)
	{
		if (HasPendingDecision())
		{
			return EditorDocumentWorkflowResult::ConfirmationRequired;
		}

		return AdvanceExit();
	}

	m_pendingDocumentId = {};
	m_discardedForExit.clear();
	m_exitPending = true;
	return AdvanceExit();
}

EditorDocumentWorkflowResult EditorDocumentWorkflow::ResolvePending(
	EditorDocumentCloseDecision decision)
{
	if (!HasPendingDecision())
	{
		return EditorDocumentWorkflowResult::NoAction;
	}

	const EditorDocumentId id = m_pendingDocumentId;

	if (!m_documents.FindDocument(id))
	{
		CancelPending();
		return EditorDocumentWorkflowResult::NotFound;
	}

	if (!m_exitPending)
	{
		if (decision == EditorDocumentCloseDecision::Save && !SaveDocument(id))
		{
			return EditorDocumentWorkflowResult::SaveFailed;
		}

		const EditorDocumentCloseDecision closeDecision =
			decision == EditorDocumentCloseDecision::Save
			? EditorDocumentCloseDecision::Discard
			: decision;
		const EditorDocumentCloseResult result = m_documents.CloseDocument(id, closeDecision);

		if (result == EditorDocumentCloseResult::SaveFailed)
		{
			return EditorDocumentWorkflowResult::SaveFailed;
		}

		m_pendingDocumentId = {};

		if (result == EditorDocumentCloseResult::Cancelled)
		{
			return EditorDocumentWorkflowResult::Cancelled;
		}

		return result == EditorDocumentCloseResult::Closed
			? EditorDocumentWorkflowResult::Closed
			: EditorDocumentWorkflowResult::NotFound;
	}

	if (decision == EditorDocumentCloseDecision::Cancel)
	{
		CancelPending();
		return EditorDocumentWorkflowResult::Cancelled;
	}

	if (decision == EditorDocumentCloseDecision::Save)
	{
		if (!SaveDocument(id))
		{
			return EditorDocumentWorkflowResult::SaveFailed;
		}
	}
	else
	{
		const IEditorDocument* document = m_documents.FindDocument(id);
		m_discardedForExit[id.value] = document->GetDirtyGeneration();
	}

	m_pendingDocumentId = {};
	return AdvanceExit();
}

bool EditorDocumentWorkflow::SaveDocument(EditorDocumentId id)
{
	if (m_saveDocument)
	{
		return m_saveDocument(id);
	}

	return m_documents.SaveDocument(id);
}

void EditorDocumentWorkflow::CancelPending()
{
	m_pendingDocumentId = {};
	m_exitPending = false;
	m_discardedForExit.clear();
}

EditorDocumentWorkflowResult EditorDocumentWorkflow::AdvanceExit()
{
	for (const EditorDocumentInfo& document : m_documents.GetDocuments())
	{
		if (!document.isDirty)
		{
			continue;
		}

		const auto discarded = m_discardedForExit.find(document.id.value);
		const IEditorDocument* liveDocument = m_documents.FindDocument(document.id);

		if (discarded != m_discardedForExit.end() && liveDocument &&
			discarded->second == liveDocument->GetDirtyGeneration())
		{
			continue;
		}

		m_pendingDocumentId = document.id;
		return EditorDocumentWorkflowResult::ConfirmationRequired;
	}

	m_pendingDocumentId = {};
	return EditorDocumentWorkflowResult::ExitReady;
}
