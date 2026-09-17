#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "Document/IEditorDocument.h"

class ActorImprintSystem;
class ActorImprintReloadResult;
struct AssetChange;

struct EditorDocumentId
{
	std::uint64_t value = 0;
	bool IsValid() const { return value != 0; }
	bool operator==(const EditorDocumentId&) const = default;
};

enum class EditorDocumentCloseDecision
{
	Cancel,
	Save,
	Discard,
};

enum class EditorDocumentCloseResult
{
	Closed,
	Cancelled,
	SaveFailed,
	NotFound,
};

struct EditorDocumentInfo
{
	EditorDocumentId id;
	EditorDocumentType type = EditorDocumentType::Scene;
	std::string displayName;
	Guid sourceAssetGuid;
	bool isActive = false;
	bool isDirty = false;
};

class EditorDocumentManager
{
public:
	EditorDocumentId AddDocument(
		std::unique_ptr<IEditorDocument> document,
		bool activate = true);
	bool ActivateDocument(EditorDocumentId id);
	bool ActivateDocument(IEditorDocument* document);
	IEditorDocument* GetActiveDocument();
	const IEditorDocument* GetActiveDocument() const;
	IEditorDocument* FindDocument(EditorDocumentId id);
	const IEditorDocument* FindDocument(EditorDocumentId id) const;
	EditorDocumentId GetActiveDocumentId() const { return m_activeDocumentId; }
	std::vector<EditorDocumentInfo> GetDocuments() const;
	IEditorDocument* FindFirst(EditorDocumentType type);
	const IEditorDocument* FindFirst(EditorDocumentType type) const;
	bool HasOpenSourceAsset(const Guid& assetGuid) const;
	std::size_t GetDocumentCount() const { return m_documents.size(); }
	bool SaveDocument(EditorDocumentId id);

	EditorDocumentCloseResult CloseDocument(
		EditorDocumentId id,
		EditorDocumentCloseDecision decision);
	void Clear();
	void ReleaseWorkingScenesForRuntimeReload();
	ActorImprintReloadResult ReloadActorImprint(
		ActorImprintSystem& system,
		const AssetChange& change);

	void ReconcileSceneReplacements(std::span<const std::size_t> replacedIndices);

private:
	struct Entry
	{
		EditorDocumentId id;
		std::unique_ptr<IEditorDocument> document;
	};

	std::vector<Entry>::iterator FindEntry(EditorDocumentId id);
	std::vector<Entry>::const_iterator FindEntry(EditorDocumentId id) const;
	std::vector<std::unique_ptr<SceneBase>*> CollectWorkingSceneOwnerSlots();

	std::vector<Entry> m_documents;
	EditorDocumentId m_activeDocumentId;
	std::uint64_t m_nextDocumentId = 1;
};
