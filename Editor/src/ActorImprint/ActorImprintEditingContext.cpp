#include "ActorImprintEditingContext.h"

#include "Core/AtomicFileReplacement.h"
#include "Engine/ActorImprint/ActorImprint.h"
#include "Engine/ActorImprint/ActorImprintAssetDeserializer.h"
#include "Engine/ActorImprint/ActorImprintAssetSerializer.h"
#include "Engine/ActorImprint/ActorImprintDefinitionExpander.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/AssetManagerAssetReferenceContext.h"
#include "Engine/Scene/SceneBase.h"

#include <filesystem>
#include <fstream>

ActorImprintEditingContext::ActorImprintEditingContext(Guid assetGuid,
	std::string assetPath, AssetManager& assets, std::unique_ptr<SceneBase> workingScene,
	ActorImprintEditingObjectMap objectMap, nlohmann::json savedSnapshot)
	: m_assetGuid(assetGuid), m_assetPath(std::move(assetPath)), m_assets(&assets),
	  m_workingScene(std::move(workingScene)), m_objectMap(std::move(objectMap)),
	  m_savedSnapshot(std::move(savedSnapshot))
{}

ActorImprintEditingContext::~ActorImprintEditingContext()
{
	if (m_workingScene) m_workingScene->Finalize();
}

std::unique_ptr<ActorImprintEditingContext> ActorImprintEditingContext::Open(
	const Guid& assetGuid, AssetManager& assets, ActorImprintSystem& system,
	EngineContext& engineContext, ActorImprintEditingOpenError* outError)
{
	if (outError) *outError = {};
	auto fail = [&](std::string path, std::string message)
		-> std::unique_ptr<ActorImprintEditingContext>
	{
		if (outError) *outError = { std::move(path), std::move(message) };
		return nullptr;
	};
	if (!assetGuid.IsValid()) return fail({}, "ActorImprint AssetGUID must be nonzero.");
	if (engineContext.pAssetManager != &assets || engineContext.pActorImprintSystem != &system)
		return fail({}, "EngineContext does not own the supplied AssetManager and ActorImprintSystem.");
	const AssetEntry* entry = assets.GetAssetEntry(assetGuid);
	if (!entry || entry->type != AssetType::ActorImprint)
		return fail({}, "AssetGUID is not a catalogued ActorImprint.");
	const std::string assetPath = assets.GetAssetPath(assetGuid);
	if (assetPath.empty()) return fail(entry->relativePath, "ActorImprint asset path is unavailable.");

	ActorImprintLoadError loadError;
	const ActorImprintHandle handle = system.Load(assetGuid, &loadError);
	const ActorImprint* definition = system.Resolve(handle);
	if (handle.IsNull() || !definition)
		return fail(loadError.assetError.path.empty() ? entry->relativePath : loadError.assetError.path,
			loadError.message.empty() ? "ActorImprint definition could not be loaded." : loadError.message);

	auto scene = std::make_unique<SceneBase>();
	scene->Initialize(engineContext);
	ActorImprintDefinitionExpansion expansion;
	ActorImprintDefinitionExpansionError expansionError;
	if (!ActorImprintDefinitionExpander::Expand(*definition, *scene, expansion, &expansionError))
	{
		scene->Finalize();
		return fail(std::move(expansionError.path), std::move(expansionError.message));
	}
	if (!scene->EnableSingleRootClosedSubtreePolicy())
	{
		scene->Finalize();
		return fail({}, "Expanded Working Scene does not form one closed root subtree.");
	}

	ActorImprintEditingObjectMap objectMap;
	if (!objectMap.Initialize(*scene, expansion, definition->GetNextLocalObjectId()))
	{
		scene->Finalize();
		return fail({}, "Expanded Working Scene does not match the definition LocalObjectIDs.");
	}
	return std::unique_ptr<ActorImprintEditingContext>(new ActorImprintEditingContext(
		assetGuid, assetPath, assets, std::move(scene), std::move(objectMap),
		ActorImprintAssetSerializer::Serialize(*definition)));
}

std::unique_ptr<const ActorImprint> ActorImprintEditingContext::CaptureSnapshot(
	nlohmann::json& outJson, ActorImprintEditingSnapshotError* outError) const
{
	if (!m_workingScene || !m_assets)
	{
		if (outError) *outError = { ActorImprintAssetErrorCode::InvalidObjectGraph,
			InvalidLocalObjectId, {}, "ActorImprint editing context is unavailable." };
		return nullptr;
	}
	return ActorImprintEditingSnapshot::Capture(*m_workingScene, m_objectMap, *m_assets,
		outJson, outError);
}

bool ActorImprintEditingContext::Save(ActorImprintEditingSaveError* outError)
{
	m_lastSaveError = {};
	if (outError) *outError = {};
	auto fail = [&](ActorImprintEditingSaveErrorCode code, std::string path, std::string message)
	{
		m_lastSaveError = { code, std::move(path), std::move(message) };
		if (outError) *outError = m_lastSaveError;
		return false;
	};
	if (!m_assets || !m_workingScene || m_assetPath.empty())
		return fail(ActorImprintEditingSaveErrorCode::InvalidContext, m_assetPath,
			"ActorImprint editing context is unavailable.");
	if (m_hasPendingSave)
		return fail(ActorImprintEditingSaveErrorCode::InvalidContext, m_assetPath,
			"The previous ActorImprint save has not reached its reload commit boundary.");
	const AssetEntry* entry = m_assets->GetAssetEntry(m_assetGuid);
	if (!entry || entry->type != AssetType::ActorImprint ||
		std::filesystem::path(m_assets->GetAssetPath(m_assetGuid)).lexically_normal() !=
		std::filesystem::path(m_assetPath).lexically_normal())
		return fail(ActorImprintEditingSaveErrorCode::InvalidContext, m_assetPath,
			"ActorImprint catalog identity changed before save.");

	nlohmann::json serialized;
	ActorImprintEditingSnapshotError snapshotError;
	auto snapshot = CaptureSnapshot(serialized, &snapshotError);
	if (!snapshot)
		return fail(ActorImprintEditingSaveErrorCode::SnapshotFailed, snapshotError.path,
			snapshotError.message.empty() ? "Could not capture the Working Scene snapshot." : snapshotError.message);

	// DefinitionRevision identifies meaningful normalized content. Ignore only
	// that field for the comparison so repeated equivalent saves retain the
	// previous revision while any content/identity change issues a new one.
	nlohmann::json candidateContent = serialized;
	nlohmann::json savedContent = m_savedSnapshot;
	candidateContent.erase("definitionRevision");
	savedContent.erase("definitionRevision");
	if (candidateContent == savedContent && m_savedSnapshot.contains("definitionRevision"))
	{
		serialized["definitionRevision"] = m_savedSnapshot["definitionRevision"];
		AssetManagerAssetReferenceContext assetContext(*m_assets);
		ActorImprintAssetError revisionError;
		snapshot = ActorImprintAssetDeserializer::Deserialize(serialized, &assetContext, &revisionError);
		if (!snapshot)
			return fail(ActorImprintEditingSaveErrorCode::SnapshotFailed,
				revisionError.path, revisionError.message);
	}

	std::filesystem::path temporary;
	AtomicFileReplacementError fileError;
	const std::string bytes = serialized.dump(2) + '\n';
	if (!AtomicFileReplacement::WriteTemporary(
		std::filesystem::path(m_assetPath), bytes, temporary, &fileError))
		return fail(ActorImprintEditingSaveErrorCode::TemporaryFileFailed,
			fileError.path.string(), fileError.message);

	struct TemporaryCleanup
	{
		std::filesystem::path& path;
		~TemporaryCleanup() { AtomicFileReplacement::RemoveTemporary(path); }
	} cleanup{temporary};

	AssetManagerAssetReferenceContext assetContext(*m_assets);
	ActorImprintAssetError validationError;
	auto validated = ActorImprintAssetDeserializer::Load(
		temporary.string(), &assetContext, &validationError);
	if (!validated)
		return fail(ActorImprintEditingSaveErrorCode::TemporaryValidationFailed,
			validationError.path, validationError.message);
	const nlohmann::json repeated = ActorImprintAssetSerializer::Serialize(*validated);
	if (repeated != serialized)
		return fail(ActorImprintEditingSaveErrorCode::TemporaryValidationFailed,
			temporary.string(), "Temporary Serialize-Deserialize-Serialize verification changed the asset.");

	std::ifstream originalFile(m_assetPath, std::ios::binary);
	if (!originalFile)
		return fail(ActorImprintEditingSaveErrorCode::InvalidContext, m_assetPath,
			"Could not read the original ActorImprint before replacement.");
	const std::string originalBytes(
		(std::istreambuf_iterator<char>(originalFile)), std::istreambuf_iterator<char>());
	if (!originalFile.good() && !originalFile.eof())
		return fail(ActorImprintEditingSaveErrorCode::InvalidContext, m_assetPath,
			"Could not read the complete original ActorImprint before replacement.");
	originalFile.close();

	if (!AtomicFileReplacement::Replace(temporary,
		std::filesystem::path(m_assetPath), &fileError))
		return fail(ActorImprintEditingSaveErrorCode::AtomicReplaceFailed,
			fileError.path.string(), fileError.message);
	temporary.clear();
	m_preSaveBytes = originalBytes;
	m_preSaveSnapshot = m_savedSnapshot;
	m_hasPendingSave = true;

	// The content-only notification cannot be rejected by an unrelated catalog
	// scan. This single-threaded context prevalidated the identity above.
	if (!m_assets->NotifyAssetContentReplaced(m_assetGuid))
	{
		ActorImprintEditingSaveError rollbackError;
		if (!RollbackPendingSave(&rollbackError))
			return fail(ActorImprintEditingSaveErrorCode::RollbackFailed,
				rollbackError.path, rollbackError.message);
		return fail(ActorImprintEditingSaveErrorCode::NotificationFailed, m_assetPath,
			"Saved asset is no longer present in the active catalog; the original file was restored.");
	}
	m_savedSnapshot = std::move(serialized);
	return true;
}

void ActorImprintEditingContext::CommitPendingSave()
{
	m_preSaveBytes.clear();
	m_preSaveSnapshot = {};
	m_hasPendingSave = false;
}

bool ActorImprintEditingContext::RollbackPendingSave(ActorImprintEditingSaveError* outError)
{
	if (outError) *outError = {};
	if (!m_hasPendingSave) return true;
	std::filesystem::path temporary;
	AtomicFileReplacementError fileError;
	if (!AtomicFileReplacement::WriteTemporary(
		std::filesystem::path(m_assetPath), m_preSaveBytes, temporary, &fileError) ||
		!AtomicFileReplacement::Replace(
			temporary, std::filesystem::path(m_assetPath), &fileError))
	{
		AtomicFileReplacement::RemoveTemporary(temporary);
		m_lastSaveError = { ActorImprintEditingSaveErrorCode::RollbackFailed,
			fileError.path.string(), fileError.message.empty()
				? "Could not restore the original ActorImprint after reload failure."
				: fileError.message };
		if (outError) *outError = m_lastSaveError;
		return false;
	}
	m_savedSnapshot = std::move(m_preSaveSnapshot);
	m_preSaveBytes.clear();
	m_preSaveSnapshot = {};
	m_hasPendingSave = false;
	return true;
}
