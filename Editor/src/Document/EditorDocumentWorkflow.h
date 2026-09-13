#pragma once

#include <functional>
#include <unordered_map>

#include "Document/EditorDocumentManager.h"

enum class EditorDocumentWorkflowResult
{
	NoAction,
	Closed,
	ConfirmationRequired,
	Cancelled,
	SaveFailed,
	ExitReady,
	NotFound,
};

// Application-layer orchestration for close and exit requests. Documents and
// dirty state remain owned by EditorDocumentManager/IEditorDocument; this type
// owns only the pending user decision needed by the UI.
class EditorDocumentWorkflow
{
public:
	explicit EditorDocumentWorkflow(EditorDocumentManager& documents)
		: m_documents(documents) {}

	bool Activate(EditorDocumentId id);
	EditorDocumentWorkflowResult RequestClose(EditorDocumentId id);
	EditorDocumentWorkflowResult RequestExit();
	EditorDocumentWorkflowResult ResolvePending(EditorDocumentCloseDecision decision);
	void SetSaveCallback(std::function<bool(EditorDocumentId)> callback)
	{
		m_saveDocument = std::move(callback);
	}

	bool HasPendingDecision() const { return m_pendingDocumentId.IsValid(); }
	bool IsExitPending() const { return m_exitPending; }
	EditorDocumentId GetPendingDocumentId() const { return m_pendingDocumentId; }
	void CancelPending();

private:
	EditorDocumentWorkflowResult AdvanceExit();
	bool SaveDocument(EditorDocumentId id);

	EditorDocumentManager& m_documents;
	EditorDocumentId m_pendingDocumentId;
	bool m_exitPending = false;
	std::unordered_map<std::uint64_t, std::uint64_t> m_discardedForExit;
	std::function<bool(EditorDocumentId)> m_saveDocument;
};
