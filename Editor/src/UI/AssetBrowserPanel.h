#pragma once


#include "Engine/Resource/AssetManager.h"

#include <string>
#include <optional>
#include <span>
#include <vector>
#include <functional>

//------------------------------------------------------------------------
// AssetBrowserPanel
// Rendering the folder tree and asset files loaded in the AssetManager.
// The folder structure which is rendered is from asset folder hierarchy,
// (below "root/assets": root is where project.101 is located.)
//------------------------------------------------------------------------

class AssetBrowserPanel
{
public:
	struct FileDrop
	{
		int x;
		int y;
		std::vector<std::filesystem::path> filePaths;
	};

	enum class AssetAction
	{
		None,
		Open,
		Rename,
		Delete,
		SetEditorStartup,	// Only for scene asset
		SetGameStartup,		// Only for scene asset
	};

	struct AssetActions
	{
		bool canOpen = false;
		bool canRename = false;
		bool canMove = false;
		bool canDelete = false;
		bool canSetStartup = false;
		bool editorStartup = false;
		bool gameStartup = false;
		std::string renameReason;
		std::string deleteReason;
	};

	struct Callbacks
	{
		// For refresh button
		std::function<void()> onRefresh;
		bool canRefresh = true;

		// For right-click context menu on asset icon.
		std::function<AssetActions(const Guid&)> queryActions;
		std::function<bool(AssetAction, const Guid&, std::string_view)> onAction;
		std::function<void(const Guid&, const std::filesystem::path&)> onMoveAsset;

		// For create button
		bool canCreate = true;
		std::function<bool(const std::filesystem::path&, std::string_view)> onCreateScene;
		std::function<bool(const std::filesystem::path&, std::string_view)> onCreateActorImprint;

		std::function<void(
			const std::filesystem::path&, std::span<const std::filesystem::path>)> onImportFiles;
	};

public:
	// Update the state of the asset browser panel by 
	// rebuilding the folder tree and asset list from the AssetManager.
	// Asset selection is kept if the selected asset is still present in the new folder tree.
	bool RebuildView(const AssetManager& assets);

	void Render(const Callbacks& callbacks, std::span<const FileDrop> drops);

	bool SelectFolder(const std::string& folderKey);	// Select folder and release the asset selection.
	bool SelectAsset(const Guid& assetGuid);			// Select asset (owner folder is also selected),
	void SetDiagnostic(const std::string& diagnostic) { m_diagnostic = diagnostic; }

	const Guid& GetSelectedAssetGuid() const { return m_selectedAssetGuid; }
	const std::string& GetSelectedFolderKey() const { return m_selectedFolderKey; }
	const std::string& GetDiagnostic() const { return m_diagnostic; }

	// Get the list of assets in the currently selected folder.
	const std::vector<AssetEntry>& GetVisibleEntries() const;

private:
	// Structure to represent an folder.
	struct FolderEntry
	{
		std::string key;					// 
		std::string name;					// Name of the folder (last part of the path)
		std::vector<std::size_t> children;	// Indices of child folders in the m_folders vector.
		std::vector<AssetEntry> assets;		// Assets in this folder.
	};

	struct MoveRequest
	{
		Guid guid;
		std::filesystem::path directory;
	};

	std::vector<FolderEntry> m_folders;			// List of folders in the asset browser
	std::string m_selectedFolderKey = "Assets";	// Key of the currently selected folder in the asset browser (being displayed)
	Guid m_selectedAssetGuid;					// GUID of the currently selected asset in the asset browser (it's information is displayed)
	std::string m_diagnostic;					// Diagnostic message to display in the asset browser panel
	std::optional<MoveRequest> m_pendingMove;

	// State for asset actions (open, rename, delete, set startup)
	AssetEntry m_actionAsset;								// Target asset for the pending action.
	AssetAction m_pendingAssetAction = AssetAction::None;	// Pending action to be performed on the target asset.
	AssetAction m_dialogAssetAction = AssetAction::None;	// Action being performed in the dialog.
	char m_renameName[256]{};								// Buffer to store the new name for the asset being renamed (without extension).
	std::string m_actionDiagnostic;							// Diagnostic message for the asset action process.

	// Asset creation state, used when the user clicks the "Create" button.
	AssetType m_createType = AssetType::Unknown;	// Type of asset being created.
	std::filesystem::path m_createDirectory;		// Where the new asset will be created (relative to the asset root).
	bool m_openCreateDialog = false;				// Whether the asset creation dialog is open.
	char m_createName[256]{};						// Store the name of the new asset being created (without extension).
	std::string m_createDiagnostic;					// Diagnostic message for the asset creation process.

private:
	// Build a folder tree from the AssetManager's asset catalog and filesystem.
	static std::vector<FolderEntry> BuildFolderTree(const AssetManager& assets);

	// get folder entry by key, return nullptr if not found
	static const FolderEntry* FindFolder(
		const std::vector<FolderEntry>& folders,
		const std::string& key);

	void DrawFolderTree(std::size_t folderIndex, const Callbacks& callbacks);

	void DrawAssetList(const Callbacks& callbacks);
	void DrawAssetMenu(const AssetEntry& entry, const Callbacks& callbacks);
	void DrawAssetActionDialog(const Callbacks & callbacks);

	void DrawSelection() const;
	void DrawCreateMenu(const Callbacks& callbacks);
	void DrawCreateDialog(const Callbacks& callbacks);
};
