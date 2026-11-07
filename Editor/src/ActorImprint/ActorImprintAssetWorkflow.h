#pragma once

#include "Document/EditorDocumentManager.h"
#include "Engine/Core/GUID/Guid.h"

#include <filesystem>
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
	static bool NormalizeFileName(std::string_view input, std::string& outFileName);

	// Create a new ActorImprint asset in the specified relative directory with the given name.
	static bool Create(
		const std::filesystem::path& relativeDirectory,	// Relative to the AssetManager's root directory.
		std::string_view name,							// Name of the new ActorImprint asset (filename only, no directory).
		AssetManager& assetManager,
		EngineContext& engineContext,
		Guid& outAssetGuid,								// Returns the AssetGUID of the newly created ActorImprint asset.
		std::string* outError = nullptr);

	// Open an ActorImprint asset in a new Document, returning the DocumentId.
	static bool OpenDocument(
		const Guid& assetGuid,
		AssetManager& assetManager,
		ActorImprintSystem& system,
		EngineContext& engineContext,
		EditorDocumentManager& documents,
		uint32_t viewportWidth,
		uint32_t viewportHeight,
		EditorDocumentId& outDocumentId);

	// Delete an ActorImprint asset, ensuring that no Document or live Instance references remain.
	static bool Delete(
		const Guid& assetGuid,
		AssetManager& assetManager,
		const ActorImprintSystem& system,
		const EditorDocumentManager& documents);
};
