#include "ActorImprintEditingContext.h"
#include "Engine/Core/Debug/Debug.h"

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

ActorImprintEditingContext::ActorImprintEditingContext(
	Guid assetGuid,
	std::string assetPath,
	AssetManager& assets,
	std::unique_ptr<SceneBase> workingScene,
	ActorImprintEditingObjectMap objectMap,
	nlohmann::json savedSnapshot)
	: m_assetGuid(assetGuid), m_assetPath(std::move(assetPath)), m_assets(&assets),
	  m_workingScene(std::move(workingScene)), m_objectMap(std::move(objectMap)),
	  m_savedSnapshot(std::move(savedSnapshot))
{}

ActorImprintEditingContext::~ActorImprintEditingContext()
{
	if (m_workingScene)
	{
		m_workingScene->Finalize();
	}
}

std::unique_ptr<ActorImprintEditingContext> ActorImprintEditingContext::Open(
	const Guid& assetGuid,
	AssetManager& assets,
	ActorImprintSystem& system,
	EngineContext& engineContext)
{
	if (!assetGuid.IsValid())
	{
		DBG("ActorImprint AssetGUID must be nonzero.");
		return nullptr;
	}

	if (engineContext.pAssetManager != &assets || engineContext.pActorImprintSystem != &system)
	{
		DBG("EngineContext does not own the supplied AssetManager and ActorImprintSystem.");
		return nullptr;
	}

	const AssetEntry* entry = assets.GetAssetEntry(assetGuid);

	if (!entry || entry->type != AssetType::ActorImprint)
	{
		DBG("AssetGUID is not a catalogued ActorImprint.");
		return nullptr;
	}

	const std::string assetPath = assets.GetAssetPath(assetGuid);

	if (assetPath.empty())
	{
		DBG("ActorImprint asset path is unavailable.");
		return nullptr;
	}

	const ActorImprintHandle handle = system.Load(assetGuid);
	const ActorImprint* definition = system.Resolve(handle);

	if (handle.IsNull() || !definition)
	{
		return nullptr;
	}

	auto scene = std::make_unique<SceneBase>();
	scene->Initialize(engineContext);
	ActorImprintDefinitionExpansion expansion;

	if (!ActorImprintDefinitionExpander::Expand(*definition, *scene, expansion))
	{
		scene->Finalize();
		return nullptr;
	}

	if (!scene->EnableSingleRootClosedSubtreePolicy())
	{
		scene->Finalize();
		DBG("Expanded Working Scene does not form one closed root subtree.");
		return nullptr;

	}

	ActorImprintEditingObjectMap objectMap;

	if (!objectMap.Initialize(*scene, expansion, definition->GetNextLocalObjectId()))
	{
		scene->Finalize();
		DBG("Expanded Working Scene does not match the definition LocalObjectIDs.");
		return nullptr;
	}

	return std::unique_ptr<ActorImprintEditingContext>(new ActorImprintEditingContext(
		assetGuid, assetPath, assets, std::move(scene), std::move(objectMap),
		ActorImprintAssetSerializer::Serialize(*definition)));
}

std::unique_ptr<const ActorImprint> ActorImprintEditingContext::CaptureSnapshot(
	nlohmann::json& outJson) const
{
	if (!m_workingScene || !m_assets)
	{
		DBG("ActorImprint editing context is unavailable.");
		return nullptr;
	}

	return ActorImprintEditingSnapshot::Capture(*m_workingScene, m_objectMap, *m_assets,
		outJson);
}

bool ActorImprintEditingContext::Save()
{
	if (!m_assets || !m_workingScene || m_assetPath.empty())
	{
		DBG("ActorImprint editing context is unavailable.");
		return false;
	}

	if (m_hasPendingSave)
	{
		DBG("The previous ActorImprint save has not reached its reload commit boundary.");
		return false;
	}

	const AssetEntry* entry = m_assets->GetAssetEntry(m_assetGuid);

	if (!entry || entry->type != AssetType::ActorImprint ||
		std::filesystem::path(m_assets->GetAssetPath(m_assetGuid)).lexically_normal() !=
		std::filesystem::path(m_assetPath).lexically_normal())
	{
		DBG("ActorImprint catalog identity changed before save.");
		return false;
	}

	nlohmann::json serialized;
	auto snapshot = CaptureSnapshot(serialized);

	if (!snapshot)
	{
		return false;
	}

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
		snapshot = ActorImprintAssetDeserializer::Deserialize(serialized, &assetContext);

		if (!snapshot)
		{
			return false;
		}
	}

	std::filesystem::path temporary;

	const std::string bytes = serialized.dump(2) + '\n';

	if (!AtomicFileReplacement::WriteTemporary(
		std::filesystem::path(m_assetPath), bytes, temporary))
	{
		DBG("ActorImprint file operation failed.");
		return false;
	}

	struct TemporaryCleanup
	{
		std::filesystem::path& path;
		~TemporaryCleanup() { AtomicFileReplacement::RemoveTemporary(path); }
	} cleanup{temporary};

	AssetManagerAssetReferenceContext assetContext(*m_assets);
	auto validated = ActorImprintAssetDeserializer::Load(temporary.string(), &assetContext);

	if (!validated)
	{
		return false;
	}

	const nlohmann::json repeated = ActorImprintAssetSerializer::Serialize(*validated);

	if (repeated != serialized)
	{
		DBG("Temporary Serialize-Deserialize-Serialize verification changed the asset.");
		return false;
	}

	std::ifstream originalFile(m_assetPath, std::ios::binary);

	if (!originalFile)
	{
		DBG("Could not read the original ActorImprint before replacement.");
		return false;
	}

	const std::string originalBytes((std::istreambuf_iterator<char>(originalFile)), std::istreambuf_iterator<char>());

	if (!originalFile.good() && !originalFile.eof())
	{
		DBG("Could not read the complete original ActorImprint before replacement.");
		return false;
	}

	originalFile.close();

	if (!AtomicFileReplacement::Replace(temporary,
		std::filesystem::path(m_assetPath)))
	{
		DBG("ActorImprint file operation failed.");
		return false;
	}

	temporary.clear();
	m_preSaveBytes = originalBytes;
	m_preSaveSnapshot = m_savedSnapshot;
	m_hasPendingSave = true;

	// The content-only notification cannot be rejected by an unrelated catalog
	// scan. This single-threaded context prevalidated the identity above.
	if (!m_assets->NotifyAssetContentReplaced(m_assetGuid))
	{
		if (!RollbackPendingSave())
		{
			return false;
		}

		DBG("Saved asset is no longer present in the active catalog; the original file was restored.");
		return false;
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

bool ActorImprintEditingContext::RollbackPendingSave()
{
	if (!m_hasPendingSave)
	{
		return true;
	}

	std::filesystem::path temporary;

	if (!AtomicFileReplacement::WriteTemporary(
		std::filesystem::path(m_assetPath), m_preSaveBytes, temporary) ||
		!AtomicFileReplacement::Replace(
			temporary, std::filesystem::path(m_assetPath)))
	{
		AtomicFileReplacement::RemoveTemporary(temporary);
		DBG("Could not restore the original ActorImprint after reload failure.");
		return false;
	}

	m_savedSnapshot = std::move(m_preSaveSnapshot);
	m_preSaveBytes.clear();
	m_preSaveSnapshot = {};
	m_hasPendingSave = false;
	return true;
}
