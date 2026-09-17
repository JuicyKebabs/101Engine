#include "Document/EditorDocumentManager.h"

#include <algorithm>

#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Scene/SceneBase.h"

EditorDocumentId EditorDocumentManager::AddDocument(
	std::unique_ptr<IEditorDocument> document,
	bool activate)
{
	if (!document)
	{
		return {};
	}

	const Guid sourceAsset = document->GetSourceAssetGuid();

	if (sourceAsset.IsValid())
	{
		for (const Entry& entry : m_documents)
		{
			if (entry.document->GetType() != document->GetType() ||
				entry.document->GetSourceAssetGuid() != sourceAsset)
			{
				continue;
			}

			if (activate)
			{
				m_activeDocumentId = entry.id;
			}

			return entry.id;
		}
	}

	const EditorDocumentId id{m_nextDocumentId++};
	m_documents.push_back({id, std::move(document)});

	if (activate || !m_activeDocumentId.IsValid())
	{
		m_activeDocumentId = id;
	}

	return id;
}

bool EditorDocumentManager::ActivateDocument(EditorDocumentId id)
{
	if (FindEntry(id) == m_documents.end())
	{
		return false;
	}

	m_activeDocumentId = id;
	return true;
}

bool EditorDocumentManager::ActivateDocument(IEditorDocument* document)
{
	if (!document)
	{
		return false;
	}

	for (const Entry& entry : m_documents)
	{
		if (entry.document.get() != document)
		{
			continue;
		}

		m_activeDocumentId = entry.id;
		return true;
	}

	return false;
}

IEditorDocument* EditorDocumentManager::GetActiveDocument()
{
	auto entry = FindEntry(m_activeDocumentId);
	return entry == m_documents.end() ? nullptr : entry->document.get();
}

const IEditorDocument* EditorDocumentManager::GetActiveDocument() const
{
	auto entry = FindEntry(m_activeDocumentId);
	return entry == m_documents.end() ? nullptr : entry->document.get();
}

IEditorDocument* EditorDocumentManager::FindDocument(EditorDocumentId id)
{
	auto entry = FindEntry(id);
	return entry == m_documents.end() ? nullptr : entry->document.get();
}

const IEditorDocument* EditorDocumentManager::FindDocument(EditorDocumentId id) const
{
	auto entry = FindEntry(id);
	return entry == m_documents.end() ? nullptr : entry->document.get();
}

std::vector<EditorDocumentInfo> EditorDocumentManager::GetDocuments() const
{
	std::vector<EditorDocumentInfo> documents;
	documents.reserve(m_documents.size());

	for (const Entry& entry : m_documents)
	{
		documents.push_back({
			.id = entry.id,
			.type = entry.document->GetType(),
			.displayName = std::string(entry.document->GetDisplayName()),
			.sourceAssetGuid = entry.document->GetSourceAssetGuid(),
			.isActive = entry.id == m_activeDocumentId,
			.isDirty = entry.document->IsDirty(),
		});
	}

	return documents;
}

IEditorDocument* EditorDocumentManager::FindFirst(EditorDocumentType type)
{
	for (Entry& entry : m_documents)
	{
		if (entry.document->GetType() == type)
		{
			return entry.document.get();
		}
	}

	return nullptr;
}

const IEditorDocument* EditorDocumentManager::FindFirst(EditorDocumentType type) const
{
	for (const Entry& entry : m_documents)
	{
		if (entry.document->GetType() == type)
		{
			return entry.document.get();
		}
	}

	return nullptr;
}

bool EditorDocumentManager::HasOpenSourceAsset(const Guid& assetGuid) const
{
	if (!assetGuid.IsValid())
	{
		return false;
	}

	return std::any_of(m_documents.begin(), m_documents.end(), [&](const Entry& entry)
	{
		return entry.document->GetSourceAssetGuid() == assetGuid;
	});
}

bool EditorDocumentManager::SaveDocument(EditorDocumentId id)
{
	IEditorDocument* document = FindDocument(id);
	return document && document->Save();
}

EditorDocumentCloseResult EditorDocumentManager::CloseDocument(
	EditorDocumentId id,
	EditorDocumentCloseDecision decision)
{
	auto entry = FindEntry(id);

	if (entry == m_documents.end())
	{
		return EditorDocumentCloseResult::NotFound;
	}

	if (entry->document->IsDirty())
	{
		if (decision == EditorDocumentCloseDecision::Cancel)
		{
			return EditorDocumentCloseResult::Cancelled;
		}

		if (decision == EditorDocumentCloseDecision::Save && !entry->document->Save())
		{
			return EditorDocumentCloseResult::SaveFailed;
		}
	}

	const bool wasActive = entry->id == m_activeDocumentId;
	const std::size_t index = static_cast<std::size_t>(entry - m_documents.begin());
	m_documents.erase(entry);

	if (wasActive)
	{
		if (m_documents.empty())
		{
			m_activeDocumentId = {};
		}
		else
		{
			m_activeDocumentId = m_documents[std::min(index, m_documents.size() - 1)].id;
		}
	}

	return EditorDocumentCloseResult::Closed;
}

void EditorDocumentManager::Clear()
{
	m_activeDocumentId = {};
	m_documents.clear();
}

void EditorDocumentManager::ReleaseWorkingScenesForRuntimeReload()
{
	for (Entry& entry : m_documents)
	{
		entry.document->ClearCommandHistory();
		entry.document->GetSelection().Clear();
		entry.document->GetViewportContext().ResetSceneTargets();
		auto* owner = entry.document->GetWorkingSceneOwnerSlot();

		if (!owner || !*owner)
		{
			continue;
		}

		(*owner)->Finalize();
		owner->reset();
	}
}

ActorImprintReloadResult EditorDocumentManager::ReloadActorImprint(
	ActorImprintSystem& system,
	const AssetChange& change)
{
	auto owners = CollectWorkingSceneOwnerSlots();
	ActorImprintReloadResult result = system.Reload(change, std::span<std::unique_ptr<SceneBase>* const>(owners));

	if (result.status == ActorImprintReloadStatus::Reloaded)
	{
		ReconcileSceneReplacements(result.affectedSceneIndices);
	}

	// The caller may still own other Scene-derived caches. It must release them
	// and call FinalizeRetiredScenes before the result leaves scope.
	return result;
}

std::vector<std::unique_ptr<SceneBase>*> EditorDocumentManager::CollectWorkingSceneOwnerSlots()
{
	std::vector<std::unique_ptr<SceneBase>*> owners;
	owners.reserve(m_documents.size());

	for (Entry& entry : m_documents)
	{
		auto* owner = entry.document->GetWorkingSceneOwnerSlot();

		if (owner && *owner)
		{
			owners.push_back(owner);
		}
	}

	return owners;
}

void EditorDocumentManager::ReconcileSceneReplacements(
	std::span<const std::size_t> replacedIndices)
{
	auto owners = CollectWorkingSceneOwnerSlots();

	for (const std::size_t index : replacedIndices)
	{
		if (index >= owners.size())
		{
			continue;
		}

		for (Entry& entry : m_documents)
		{
			if (entry.document->GetWorkingSceneOwnerSlot() != owners[index])
			{
				continue;
			}

			entry.document->ClearCommandHistory();
			entry.document->GetSelection().Revalidate(entry.document->GetWorkingScene());
			entry.document->GetViewportContext().Revalidate(entry.document->GetWorkingScene());
			entry.document->MarkDirty();
			break;
		}
	}
}

std::vector<EditorDocumentManager::Entry>::iterator
EditorDocumentManager::FindEntry(EditorDocumentId id)
{
	return std::find_if(m_documents.begin(), m_documents.end(),
		[id](const Entry& entry) { return entry.id == id; });
}

std::vector<EditorDocumentManager::Entry>::const_iterator
EditorDocumentManager::FindEntry(EditorDocumentId id) const
{
	return std::find_if(m_documents.begin(), m_documents.end(),
		[id](const Entry& entry) { return entry.id == id; });
}
