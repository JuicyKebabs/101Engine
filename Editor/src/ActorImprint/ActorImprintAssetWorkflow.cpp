#include "ActorImprintAssetWorkflow.h"
#include "Engine/Core/Debug/Debug.h"

#include "ActorImprint/ActorImprintEditingContext.h"
#include "ActorImprint/ActorImprintEditingSnapshot.h"
#include "Document/ActorImprintEditorDocument.h"
#include "Engine/ActorImprint/ActorImprintAssetDeserializer.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/AssetManagerAssetReferenceContext.h"
#include "Engine/Resource/MetaFile.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <unordered_set>

namespace
{
	namespace fs = std::filesystem;

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
		return value;
	}

	bool IsReservedWindowsName(const fs::path& stem)
	{
		std::wstring base = Lower(stem.filename().wstring());

		if (const auto dot = base.find(L'.'); dot != std::wstring::npos)
		{
			base.resize(dot);
		}

		static const std::unordered_set<std::wstring> reserved{
			L"con", L"prn", L"aux", L"nul", L"clock$",
			L"com1", L"com2", L"com3", L"com4", L"com5", L"com6", L"com7", L"com8", L"com9",
			L"lpt1", L"lpt2", L"lpt3", L"lpt4", L"lpt5", L"lpt6", L"lpt7", L"lpt8", L"lpt9",
		};
		return reserved.contains(base);
	}

	bool WriteNewFile(const fs::path& path, std::string_view bytes)
	{
		const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		bool succeeded = true;
		std::size_t offset = 0;
		while (offset < bytes.size())
		{
			const DWORD requested = static_cast<DWORD>(
				(std::min)(bytes.size() - offset, static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
			DWORD written = 0;

			if (!WriteFile(file, bytes.data() + offset, requested, &written, nullptr) || written != requested)
			{
				succeeded = false;
				break;
			}

			offset += written;
		}

		if (succeeded)
		{
			succeeded = FlushFileBuffers(file) != FALSE;
		}

		if (!CloseHandle(file))
		{
			succeeded = false;
		}

		if (!succeeded)
		{
			DeleteFileW(path.c_str());
		}

		return succeeded;
	}

	bool MoveNew(const fs::path& source, const fs::path& destination)
	{
		return MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
	}

	fs::path UniqueSibling(const fs::path& destination, const wchar_t* suffix)
	{
		return destination.parent_path() / (destination.filename().wstring() + L".101-" +
			fs::path(GuidGenerator::Generate().ToString()).wstring() + suffix);
	}

	bool HasCaseInsensitiveCollision(const fs::path& root, const fs::path& fileName)
	{
		std::error_code error;
		const std::wstring wanted = Lower(fileName.filename().wstring());
		const std::wstring wantedMeta = wanted + L".meta";

		for (fs::directory_iterator it(root, error), end; !error && it != end; it.increment(error))
		{
			const std::wstring existing = Lower(it->path().filename().wstring());

			if (existing == wanted || existing == wantedMeta)
			{
				return true;
			}
		}

		return error || fs::exists(root / fileName, error) || fs::exists((root / fileName).wstring() + L".meta", error);
	}

	bool RestorePair(
		const fs::path& stagedData,
		const fs::path& data,
		const fs::path& stagedMeta,
		const fs::path& meta)
	{
		bool restored = true;
		std::error_code error;

		if (fs::exists(stagedMeta, error))
		{
			restored = MoveNew(stagedMeta, meta) && restored;
		}

		if (error)
		{
			restored = false;
		}

		error.clear();

		if (fs::exists(stagedData, error))
		{
			restored = MoveNew(stagedData, data) && restored;
		}

		if (error)
		{
			restored = false;
		}

		return restored;
	}
}

bool ActorImprintAssetWorkflow::NormalizeFileName(
	std::string_view input,
	std::string& outFileName)
{
	outFileName.clear();

	if (input.empty())
	{
		DBG("ActorImprint name must not be empty.");
		return false;
	}

	const fs::path supplied{std::string(input)};

	if (supplied.has_parent_path() || supplied.filename() != supplied)
	{
		DBG("ActorImprint name must be one filename without a directory.");
		return false;
	}

	const std::wstring raw = supplied.filename().wstring();

	if (raw.empty() || raw == L"." || raw == L".." || raw.back() == L' ' || raw.back() == L'.' ||
		std::any_of(raw.begin(), raw.end(), [](wchar_t c)
		{
			return c < 32 || c == L'<' || c == L'>' || c == L':' || c == L'"' ||
				c == L'/' || c == L'\\' || c == L'|' || c == L'?' || c == L'*';
		}))
	{
		DBG("ActorImprint name contains characters Windows filenames do not allow.");
		return false;
	}

	fs::path normalized = supplied;

	if (Lower(normalized.extension().wstring()) == L".imprint")
	{
		normalized.replace_extension(L".imprint");
	}
	else
	{
		normalized += L".imprint";
	}

	const fs::path logicalStem = normalized.stem();

	if (logicalStem.empty())
	{
		DBG("ActorImprint name must contain a non-empty stem.");
		return false;
	}

	if (IsReservedWindowsName(logicalStem))
	{
		DBG("ActorImprint name is reserved by Windows.");
		return false;
	}

	if (normalized.filename().wstring().size() > 255)
	{
		DBG("ActorImprint filename exceeds the Windows component limit.");
		return false;
	}

	outFileName = normalized.filename().string();
	return true;
}

bool ActorImprintAssetWorkflow::IsManagedAssetPath(std::string_view relativePath)
{
	const fs::path normalized = fs::path(relativePath).lexically_normal();

	if (normalized.empty() || normalized.is_absolute())
	{
		return false;
	}

	auto component = normalized.begin();

	if (component == normalized.end() ||
		Lower(component->wstring()) !=
		Lower(fs::path(ManagedDirectory()).wstring()))
	{
		return false;
	}

	return ++component != normalized.end();

}

bool ActorImprintAssetWorkflow::Create(
	std::string_view name,
	AssetManager& assets,
	EngineContext& engineContext,
	Guid& outAssetGuid)
{
	outAssetGuid = {};
	std::string fileName;

	if (!NormalizeFileName(name, fileName))
	{
		return false;
	}

	const fs::path root = fs::path(assets.GetAssetRoot());

	if (root.empty() || !fs::is_directory(root) || engineContext.pAssetManager != &assets)
	{
		DBG("Asset catalog root or EngineContext is unavailable.");
		return false;
	}

	const fs::path managedDirectory = root / fs::path(ManagedDirectory());
	std::error_code directoryError;
	fs::create_directories(managedDirectory, directoryError);

	if (directoryError || !fs::is_directory(managedDirectory, directoryError))
	{
		DBG("Could not prepare the ActorImprints asset directory.");
		return false;
	}

	const fs::path destination = fs::absolute(managedDirectory / fileName).lexically_normal();

	if (HasCaseInsensitiveCollision(managedDirectory, destination.filename()))
	{
		DBG("An asset or metadata sidecar already uses this name, ignoring case.");
		return false;
	}

	nlohmann::json serialized;

	if (!ActorImprintEditingSnapshot::CreateDefault(assets, engineContext, serialized))
	{
		return false;
	}

	const std::string bytes = serialized.dump(2) + '\n';
	const Guid assetGuid = GuidGenerator::Generate();

	if (!assetGuid.IsValid())
	{
		DBG("Could not allocate an AssetGUID.");
		return false;
	}

	const fs::path temporaryData = UniqueSibling(destination, L".tmp");
	const fs::path temporaryMetaBase = UniqueSibling(destination, L".meta-tmp");
	const fs::path temporaryMeta = temporaryMetaBase.wstring() + L".meta";
	const fs::path destinationMeta = destination.wstring() + L".meta";

	if (!WriteNewFile(temporaryData, bytes) ||
		!MetaFile::Save(temporaryMetaBase.string(), assetGuid, MetaFile::WriteMode::CreateNew))
	{
		DeleteFileW(temporaryData.c_str());
		DeleteFileW(temporaryMeta.c_str());
		DBG("Could not stage the ActorImprint and metadata files.");
		return false;

	}

	AssetManagerAssetReferenceContext assetContext(assets);

	if (!ActorImprintAssetDeserializer::Load(temporaryData.string(), &assetContext))
	{
		DeleteFileW(temporaryData.c_str());
		DeleteFileW(temporaryMeta.c_str());
		return false;
	}

	if (!MoveNew(temporaryMeta, destinationMeta))
	{
		DeleteFileW(temporaryData.c_str());
		DeleteFileW(temporaryMeta.c_str());
		DBG("Could not publish ActorImprint metadata.");
		return false;
	}

	if (!MoveNew(temporaryData, destination))
	{
		const bool recovered = MoveNew(destinationMeta, temporaryMeta);
		DeleteFileW(temporaryData.c_str());
		DeleteFileW(temporaryMeta.c_str());
		const char* failureMessage = recovered
			? "Could not publish the ActorImprint file; staged metadata was removed."
			: "Could not publish the ActorImprint file or recover its metadata sidecar.";
		DBG("%s", failureMessage);
		return false;
	}

	const std::string relativePath = (fs::path(ManagedDirectory()) / fileName).generic_string();

	if (!assets.NotifyAssetChanged(relativePath))
	{
		bool recovered = MoveNew(destination, temporaryData);
		recovered = MoveNew(destinationMeta, temporaryMeta) && recovered;
		DeleteFileW(temporaryData.c_str());
		DeleteFileW(temporaryMeta.c_str());
		const char* failureMessage = recovered
			? "Asset catalog update failed."
			: "Catalog rejected creation and the published pair could not be fully recovered.";
		DBG("%s", failureMessage);
		return false;
	}

	outAssetGuid = assetGuid;
	return true;
}

bool ActorImprintAssetWorkflow::OpenDocument(
	const Guid& assetGuid,
	AssetManager& assets,
	ActorImprintSystem& system,
	EngineContext& engineContext,
	EditorDocumentManager& documents,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	EditorDocumentId& outDocumentId)
{
	outDocumentId = {};
	auto context = ActorImprintEditingContext::Open(assetGuid, assets, system, engineContext);

	if (!context)
	{
		return false;
	}

	outDocumentId = documents.AddDocument(std::make_unique<ActorImprintEditorDocument>(
		std::move(context), viewportWidth, viewportHeight));
	return outDocumentId.IsValid();
}

bool ActorImprintAssetWorkflow::Delete(
	const Guid& assetGuid,
	AssetManager& assets,
	const ActorImprintSystem& system,
	const EditorDocumentManager& documents)
{
	const AssetEntry* entry = assets.GetAssetEntry(assetGuid);

	if (!entry || entry->type != AssetType::ActorImprint)
	{
		DBG("AssetGUID is not a catalogued ActorImprint.");
		return false;
	}

	if (documents.HasOpenSourceAsset(assetGuid))
	{
		DBG("Close the ActorImprint Document before deleting this asset.");
		return false;
	}

	if (system.GetLiveInstanceCount(assetGuid) != 0)
	{
		DBG("Remove every loaded Scene Instance before deleting this asset.");
		return false;
	}

	const std::string relativePath = entry->relativePath;
	const fs::path destination = fs::absolute(assets.GetAssetPath(assetGuid)).lexically_normal();
	const fs::path destinationMeta = destination.wstring() + L".meta";
	const auto metadataGuid = MetaFile::TryLoad(destination.string());

	if (!metadataGuid || *metadataGuid != assetGuid)
	{
		DBG("ActorImprint metadata identity does not match the catalog.");
		return false;
	}

	const fs::path stagedData = UniqueSibling(destination, L".delete-tmp");
	const fs::path stagedMeta = UniqueSibling(destination, L".delete-meta-tmp");

	if (!MoveNew(destination, stagedData))
	{
		DBG("Could not stage the ActorImprint for deletion.");
		return false;
	}

	if (!MoveNew(destinationMeta, stagedMeta))
	{
		const bool recovered = MoveNew(stagedData, destination);
		const char* failureMessage = recovered
			? "Could not stage metadata; the asset was restored."
			: "Could not stage metadata or restore the asset file.";
		DBG("%s", failureMessage);
		return false;
	}

	if (!assets.NotifyAssetChanged(relativePath))
	{
		const bool recovered = RestorePair(stagedData, destination, stagedMeta, destinationMeta);
		const char* failureMessage = recovered
			? "Asset catalog update failed."
			: "Catalog rejected deletion and the asset pair could not be restored.";
		DBG("%s", failureMessage);
		return false;
	}

	// The catalog no longer exposes the asset. Cleanup failure can leave only
	// unknown-extension transaction files, never a partially discoverable asset.
	DeleteFileW(stagedData.c_str());
	DeleteFileW(stagedMeta.c_str());
	return true;
}
