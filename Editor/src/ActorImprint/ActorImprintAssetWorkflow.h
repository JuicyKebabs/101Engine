#pragma once

#include "Document/EditorDocumentManager.h"
#include "Engine/Core/GUID/Guid.h"

#include <string>
#include <string_view>

class ActorImprintSystem;
class AssetManager;
struct EngineContext;

// Non-UI workflow boundary for ActorImprint asset operations. Panels supply
// user intent; this class owns validation, two-file publication and Document /
// live-Instance reference checks.
class ActorImprintAssetWorkflow
{
public:
	static constexpr std::string_view ManagedDirectory() { return "ActorImprints"; }
	static bool IsManagedAssetPath(std::string_view relativePath);
	static bool NormalizeFileName(std::string_view input, std::string& outFileName);
	static bool Create(
		std::string_view name,
		AssetManager& assets,
		EngineContext& engineContext,
		Guid& outAssetGuid);
	static bool OpenDocument(
		const Guid& assetGuid,
		AssetManager& assets,
		ActorImprintSystem& system,
		EngineContext& engineContext,
		EditorDocumentManager& documents,
		uint32_t viewportWidth,
		uint32_t viewportHeight,
		EditorDocumentId& outDocumentId);
	static bool Delete(
		const Guid& assetGuid,
		AssetManager& assets,
		const ActorImprintSystem& system,
		const EditorDocumentManager& documents);
};
