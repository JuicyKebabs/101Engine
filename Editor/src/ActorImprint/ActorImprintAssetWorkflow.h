#pragma once

#include "Document/EditorDocumentManager.h"
#include "Engine/Core/GUID/Guid.h"

#include <string>
#include <string_view>

class ActorImprintSystem;
class AssetManager;
struct EngineContext;

enum class ActorImprintAssetWorkflowErrorCode
{
	None,
	InvalidName,
	ReservedName,
	NameCollision,
	InvalidAsset,
	OpenDocumentReference,
	LiveInstanceReference,
	FilesystemFailure,
	CatalogFailure,
	OpenFailure,
	RecoveryFailure,
};

struct ActorImprintAssetWorkflowError
{
	ActorImprintAssetWorkflowErrorCode code = ActorImprintAssetWorkflowErrorCode::None;
	std::string path;
	std::string message;
};

// Non-UI workflow boundary for ActorImprint asset operations. Panels supply
// user intent; this class owns validation, two-file publication and Document /
// live-Instance reference checks.
class ActorImprintAssetWorkflow
{
public:
	static constexpr std::string_view ManagedDirectory() { return "ActorImprints"; }
	static bool IsManagedAssetPath(std::string_view relativePath);
	static bool NormalizeFileName(std::string_view input, std::string& outFileName,
		ActorImprintAssetWorkflowError* outError = nullptr);
	static bool Create(std::string_view name, AssetManager& assets,
		EngineContext& engineContext, Guid& outAssetGuid,
		ActorImprintAssetWorkflowError* outError = nullptr);
	static bool OpenDocument(const Guid& assetGuid, AssetManager& assets,
		ActorImprintSystem& system, EngineContext& engineContext,
		EditorDocumentManager& documents, uint32_t viewportWidth, uint32_t viewportHeight,
		EditorDocumentId& outDocumentId,
		ActorImprintAssetWorkflowError* outError = nullptr);
	static bool Delete(const Guid& assetGuid, AssetManager& assets,
		const ActorImprintSystem& system, const EditorDocumentManager& documents,
		ActorImprintAssetWorkflowError* outError = nullptr);
};
