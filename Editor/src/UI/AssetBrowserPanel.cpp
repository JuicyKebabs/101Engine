#include "AssetBrowserPanel.h"

#include "Engine/Resource/BuiltinMeshAssets.h"
#include "imgui.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace
{

namespace fs = std::filesystem;

std::string DisplayPath(const fs::path& path)
{
	// Convert the path to a UTF-8 string for display purposes.
	const std::u8string text = path.generic_u8string();
	return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

std::string FolderKey(const fs::path& relativePath)
{
	// Return root folder key if the relative path is empty
	if (relativePath.empty())
	{
		return "Assets";
	}

	// Match AssetManager's catalog path convention.
	return "Assets/" + relativePath.generic_string();
}

const char* AssetTypeName(AssetType type)
{
	// Convert AssetType enum to a string.
	switch (type)
	{
	case AssetType::Mesh: return "Mesh";
	case AssetType::Texture: return "Texture";
	case AssetType::Audio: return "Audio";
	case AssetType::ActorImprint: return "ActorImprint";
	case AssetType::Scene: return "Scene";
	default: return "Unknown";
	}
}

void DrawAssetIcon(const ImVec2& position, float size, AssetType type)
{
	ImU32 color = IM_COL32(130, 140, 155, 255);
	const char* label = "FILE";

	switch (type)
	{
	case AssetType::Mesh:
		color = IM_COL32(80, 155, 220, 255);
		label = "MESH";
		break;
	case AssetType::Texture:
		color = IM_COL32(165, 110, 210, 255);
		label = "TEX";
		break;
	case AssetType::Audio:
		color = IM_COL32(75, 165, 120, 255);
		label = "AUDIO";
		break;
	case AssetType::Scene:
		color = IM_COL32(205, 155, 65, 255);
		label = "SCENE";
		break;
	case AssetType::ActorImprint:
		color = IM_COL32(210, 115, 90, 255);
		label = "IMPRINT";
		break;
	default:
		break;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float pageWidth = size * 0.8f;
	const float fold = size * 0.2f;

	const ImVec2 topLeft(position.x + (size - pageWidth) * 0.5f, position.y);
	const ImVec2 foldTop(topLeft.x + pageWidth - fold, topLeft.y);
	const ImVec2 foldRight(topLeft.x + pageWidth, topLeft.y + fold);
	const ImVec2 bottomRight(topLeft.x + pageWidth, topLeft.y + size);
	const ImVec2 bottomLeft(topLeft.x, topLeft.y + size);
	const ImVec2 foldCorner(foldTop.x, foldRight.y);

	drawList->PathLineTo(topLeft);
	drawList->PathLineTo(foldTop);
	drawList->PathLineTo(foldRight);
	drawList->PathLineTo(bottomRight);
	drawList->PathLineTo(bottomLeft);
	drawList->PathFillConvex(color);

	const ImU32 foreground = IM_COL32(255, 255, 255, 255);
	drawList->AddLine(foldTop, foldCorner, foreground);
	drawList->AddLine(foldCorner, foldRight, foreground);

	const ImVec2 labelSize = ImGui::CalcTextSize(label);
	const float scale = std::min(1.0f, (pageWidth - 8.0f) / labelSize.x);
	const ImVec2 labelPosition(
		topLeft.x + (pageWidth - labelSize.x * scale) * 0.5f,
		topLeft.y + size * 0.58f);

	drawList->AddText(
		ImGui::GetFont(),
		ImGui::GetFontSize() * scale,
		labelPosition,
		foreground,
		label);
}

}

std::vector<AssetBrowserPanel::FolderEntry> AssetBrowserPanel::BuildFolderTree(const AssetManager& assetManager)
{
	// get aset root path from AssetManager(root/Assets)
	const fs::path root(assetManager.GetAssetRoot());

	if (root.empty() || !fs::is_directory(root))
	{
		throw std::runtime_error("Asset root is unavailable.");
	}

	std::vector<FolderEntry> folders; // List of folders in the asset browser(return value)

	// Entry for the root folder
	FolderEntry projectRoot;
	projectRoot.key = "Assets";
	projectRoot.name = "Assets";
	folders.push_back(std::move(projectRoot));

	// Entry for the Built-in folder (not existing actually, but display for built-in assets operation)
	FolderEntry builtinRoot;
	builtinRoot.key = "BuiltIn";
	builtinRoot.name = "BuiltIn";
	folders.push_back(std::move(builtinRoot));

	std::size_t builtinRootIndex = folders.size() - 1;

	// Store checked folder keys and their indices in the folders vector for quick lookup.
	std::unordered_map<std::string, std::size_t> folderIndices;
	folderIndices.emplace("Assets", 0);

	// Collect all folders and assets from the filesystem and the AssetManager's catalog.
	for (const fs::directory_entry& entry: fs::recursive_directory_iterator(root))
	{
		// Relative path from the asset root to the current entry (file or folder: without "Assets/").
		const fs::path relative = entry.path().lexically_relative(root);

		// Indices of the parent folder which already processed (must exist in the folderIndices map).
		const fs::path parentPath = relative.parent_path();
		const std::size_t parentIndex = folderIndices.at(FolderKey(parentPath));

		if (entry.is_directory())
		{// In case of the entry is directry(folder)
			FolderEntry folder;
			folder.key = FolderKey(relative);
			folder.name = DisplayPath(entry.path().filename());

			// Add the folder to the processed folder list.
			const std::size_t folderIndex = folders.size();
			folderIndices.emplace(folder.key, folderIndex);

			// Add the folder to the list of return value and update the parent's children list.
			folders.push_back(std::move(folder));
			folders[parentIndex].children.push_back(folderIndex);
		}
		else if (entry.is_regular_file())
		{// In case of the entry is file(asset)
			// Get the asset entry from the AssetManager by its relative path.
			const AssetEntry* entry = assetManager.GetAssetEntryByPath(relative.generic_string());

			// Store the asset entry in the parent folder's assets list if it exists.
			if (entry)
			{
				folders[parentIndex].assets.push_back(*entry);
			}
		}
	}

	// Add built-in mesh assets to the "BuiltIn" folder.
	for (const BuiltinMeshAsset& builtin : GetBuiltinMeshAssets())
	{
		// Only mesh is exists in the built-in assets currently, 
		// but this can be extended in the future (materials , textures, etc.).
		const AssetEntry* entry = assetManager.GetAssetEntry(builtin.guid);

		if (entry && entry->type == AssetType::Mesh)
		{
			folders[builtinRootIndex].assets.push_back(*entry);
		}
	}

	// Sort the folders and assets alphabetically by name.
	for (FolderEntry& folder : folders)
	{
		std::sort(
			folder.children.begin(), folder.children.end(),
			[&folders](std::size_t left, std::size_t right)
			{
				return folders[left].name < folders[right].name;
			});

		std::sort(
			folder.assets.begin(), folder.assets.end(),
			[](const AssetEntry& left, const AssetEntry& right)
			{
				return left.relativePath < right.relativePath;
			});
	}

	return folders;
}

const AssetBrowserPanel::FolderEntry* AssetBrowserPanel::FindFolder(
	const std::vector<FolderEntry>& folders,
	const std::string& key)
{
	// Get entry by key.
	const auto found = std::find_if(
		folders.begin(), folders.end(),
		[&key](const FolderEntry& folder)
		{
			return folder.key == key;
		});

	return found == folders.end() ? nullptr : &*found;
}

bool AssetBrowserPanel::RebuildView(const AssetManager& assets)
{
	try
	{
		// Build a new folder tree from the AssetManager's catalog and filesystem
		// to be displayed in the asset browser panel.
		std::vector<FolderEntry> candidate = BuildFolderTree(assets);

		std::string selectedFolder = m_selectedFolderKey;
		Guid selectedAsset;

		// Find the entry of the currently selected asset in the new folder tree.
		if (m_selectedAssetGuid.IsValid())
		{
			for (const FolderEntry& folder : candidate)
			{
				const auto found = std::find_if(
					folder.assets.begin(), folder.assets.end(),
					[this](const AssetEntry& entry)
					{
						return entry.guid == m_selectedAssetGuid;
					});

				if (found != folder.assets.end())
				{
					selectedFolder = folder.key;
					selectedAsset = found->guid;
					break;
				}
			}
		}

		// If the currently selected folder is not found in the new folder tree,
		// traverse up the folder hierarchy until a valid folder is found or the root is reached.
		while (!FindFolder(candidate, selectedFolder))
		{
			const std::size_t separator = selectedFolder.rfind('/');

			if (separator == std::string::npos)
			{
				selectedFolder = "Assets";
			}
			else
			{
				selectedFolder.resize(separator);
			}
		}

		// Update the asset browser panel's state with the new folder tree and selection.
		m_folders.swap(candidate);
		m_selectedFolderKey.swap(selectedFolder);
		m_selectedAssetGuid = selectedAsset;
		m_diagnostic.clear();
		return true;
	}
	catch (const std::exception& error)
	{
		m_diagnostic = error.what();
		return false;
	}
}

bool AssetBrowserPanel::SelectFolder(const std::string& folderKey)
{
	if (!FindFolder(m_folders, folderKey))
	{
		return false;
	}

	if (m_selectedFolderKey != folderKey)
	{
		// Clear the selected asset when selecting a new folder.
		Guid clearedSelection{};
		m_selectedFolderKey = folderKey;
		m_selectedAssetGuid = clearedSelection;
	}

	return true;
}

bool AssetBrowserPanel::SelectAsset(const Guid& assetGuid)
{
	if (!assetGuid.IsValid())
	{
		return false;
	}

	// Find the folder containing the asset with the specified GUID and select it.
	for (const FolderEntry& folder : m_folders)
	{
		for (const AssetEntry& entry : folder.assets)
		{
			if (entry.guid == assetGuid)
			{
				m_selectedFolderKey = folder.key;
				m_selectedAssetGuid = entry.guid;
				return true;
			}
		}
	}

	return false;
}

const std::vector<AssetEntry>& AssetBrowserPanel::GetVisibleEntries() const
{
	// Get the all assets in the currently selected folder to be displayed in the asset browser panel.
	const FolderEntry* folder = FindFolder(m_folders, m_selectedFolderKey);

	if (folder)
	{
		return folder->assets;
	}

	static const std::vector<AssetEntry> empty;
	return empty;
}

void AssetBrowserPanel::DrawFolderTree(std::size_t folderIndex)
{
	const FolderEntry& folder = m_folders[folderIndex];

	ImGuiTreeNodeFlags flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth;

	if (folderIndex < 2)
	{
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}

	if (folder.children.empty())
	{
		flags |= ImGuiTreeNodeFlags_Leaf;
	}

	if (folder.key == m_selectedFolderKey)
	{
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	ImGui::PushID(folder.key.c_str());
	const bool open = ImGui::TreeNodeEx("##Folder", flags, "%s", folder.name.c_str());

	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		SelectFolder(folder.key);
	}

	if (open)
	{
		for (const std::size_t childIndex : folder.children)
		{
			DrawFolderTree(childIndex);
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

void AssetBrowserPanel::DrawAssetList()
{
	const auto& entries = GetVisibleEntries();

	if (entries.empty())
	{
		ImGui::TextDisabled("No registered assets in this folder.");
		return;
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const float availableWidth = ImGui::GetContentRegionAvail().x;

	if (availableWidth < 32.0f)
	{
		return;
	}

	const float preferredIconSize = ImGui::GetFontSize() * 3.5f;
	const float tileWidth = std::min(preferredIconSize + 32.0f, availableWidth);
	const float iconSize = std::min(preferredIconSize, tileWidth - 16.0f);
	const float labelWidth = tileWidth - 12.0f;
	const float textHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0f;
	const int columns = std::max(1, static_cast<int>((availableWidth + style.ItemSpacing.x) / (tileWidth + style.ItemSpacing.x)));

	ImVec2 tileSize;
	tileSize.x = tileWidth;
	tileSize.y = iconSize + textHeight + 20.0f;

	int column = 0;

	for (const AssetEntry& entry : entries)
	{
		if (column > 0)
		{
			ImGui::SameLine(0.0f, style.ItemSpacing.x);
		}

		ImGui::PushID(entry.guid.ToString().c_str());
		const ImVec2 tileMin = ImGui::GetCursorScreenPos();

		if (ImGui::InvisibleButton("##AssetTile", tileSize))
		{
			m_selectedAssetGuid = entry.guid;
		}

		const bool hovered = ImGui::IsItemHovered();
		const bool selected = entry.guid == m_selectedAssetGuid;

		const ImVec2 tileMax(tileMin.x + tileSize.x, tileMin.y + tileSize.y);
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		if (selected || hovered)
		{
			const ImGuiCol background = selected ? ImGuiCol_Header : ImGuiCol_HeaderHovered;

			drawList->AddRectFilled(
				tileMin,
				tileMax,
				ImGui::GetColorU32(background),
				style.FrameRounding);
		}

		const ImVec2 iconPosition(tileMin.x + (tileWidth - iconSize) * 0.5f, tileMin.y + 6.0f);
		DrawAssetIcon(iconPosition, iconSize, entry.type);

		const std::string name = DisplayPath(fs::path(entry.relativePath).filename());
		const ImVec2 namePosition(tileMin.x + 6.0f, iconPosition.y + iconSize + 6.0f);
		const ImVec4 nameClip(
			namePosition.x,
			namePosition.y,
			tileMax.x - 6.0f,
			tileMax.y - 4.0f);

		drawList->AddText(
			ImGui::GetFont(),
			ImGui::GetFontSize(),
			namePosition,
			ImGui::GetColorU32(ImGuiCol_Text),
			name.c_str(),
			nullptr,
			labelWidth,
			&nameClip);

		if (hovered)
		{
			ImGui::BeginTooltip();
			const std::string path = DisplayPath(fs::path(entry.relativePath));
			ImGui::TextUnformatted(path.c_str());
			ImGui::TextUnformatted(AssetTypeName(entry.type));
			ImGui::EndTooltip();
		}

		ImGui::PopID();
		column = (column + 1) % columns;
	}
}

void AssetBrowserPanel::DrawSelection() const
{
	for (const AssetEntry& entry : GetVisibleEntries())
	{
		if (entry.guid == m_selectedAssetGuid)
		{
			ImGui::Text("Type: %s", AssetTypeName(entry.type));
			const std::string path = DisplayPath(fs::path(entry.relativePath));
			ImGui::TextWrapped("Path: %s", path.c_str());
			ImGui::TextWrapped("GUID: %s", entry.guid.ToString().c_str());
			return;
		}
	}

	ImGui::TextDisabled("No asset selected.");
}

void AssetBrowserPanel::DrawCreateMenu(const Callbacks& callbacks)
{
	const bool projectFolder = m_selectedFolderKey == "Assets" || m_selectedFolderKey.starts_with("Assets/");
	const bool canCreate = callbacks.canCreate && projectFolder;

	const ImGuiPopupFlags flags =
	ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems;

	if (!ImGui::BeginPopupContextWindow("AssetBrowserCreateContext", flags))
	{
		return;
	}

	if (ImGui::BeginMenu("Create", canCreate))
	{
		AssetType requestedType = AssetType::Unknown;

		if (ImGui::MenuItem("Scene", nullptr, false, static_cast<bool>(callbacks.onCreateScene)))
		{
			requestedType = AssetType::Scene;
		}

		if (ImGui::MenuItem("ActorImprint", nullptr, false, static_cast<bool>(callbacks.onCreateActorImprint)))
		{
			requestedType = AssetType::ActorImprint;
		}

		if (requestedType != AssetType::Unknown)
		{
			m_createType = requestedType;
			m_createDirectory = ".";

			if (m_selectedFolderKey != "Assets")
			{
				m_createDirectory = std::filesystem::path(m_selectedFolderKey.substr(7));
			}

			m_createName[0] = '\0';
			m_createDiagnostic.clear();
			m_openCreateDialog = true;
		}

		ImGui::EndMenu();
	}

	ImGui::EndPopup();
}

void AssetBrowserPanel::DrawCreateDialog(const Callbacks& callbacks)
{
	if (m_openCreateDialog)
	{
		ImGui::OpenPopup("Create Asset");
		m_openCreateDialog = false;
	}

	if (!ImGui::BeginPopupModal("Create Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	ImGui::Text("Type: %s", AssetTypeName(m_createType));
	const std::string destination = DisplayPath(m_createDirectory);
	ImGui::TextWrapped("Folder: Assets / %s", destination.c_str());
	ImGui::InputText("Name", m_createName, sizeof(m_createName));

	if (!m_createDiagnostic.empty())
	{
		ImGui::TextWrapped("%s", m_createDiagnostic.c_str());
	}

	const auto& create = m_createType == AssetType::Scene
			? callbacks.onCreateScene
			: callbacks.onCreateActorImprint;
	const bool enabled = callbacks.canCreate && static_cast<bool>(create);

	ImGui::BeginDisabled(!enabled);
	const bool createPressed = ImGui::Button("Create");
	ImGui::EndDisabled();

	if (createPressed && enabled)
	{
		// The callback may rebuild the Browser. No folder/entry references survive here.
		m_diagnostic.clear();

		if (create(m_createDirectory, m_createName))
		{
			m_createDiagnostic.clear();
			ImGui::CloseCurrentPopup();
		}
		else
		{
			m_createDiagnostic = m_diagnostic;

			if (m_createDiagnostic.empty())
			{
				m_createDiagnostic = "Creation failed.";
			}
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Cancel"))
	{
		m_createDiagnostic.clear();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void AssetBrowserPanel::Render(const Callbacks& callbacks)
{
	const bool visible = ImGui::Begin("Assets");

	if (visible)
	{
		const bool refreshEnabled = callbacks.canRefresh && static_cast<bool>(callbacks.onRefresh);

		ImGui::BeginDisabled(!refreshEnabled);

		if (ImGui::Button("Refresh") && refreshEnabled)
		{
			callbacks.onRefresh();
		}

		ImGui::EndDisabled();
		ImGui::Separator();

		if (!m_diagnostic.empty())
		{
			ImGui::TextWrapped("%s", m_diagnostic.c_str());
			ImGui::Separator();
		}

		const ImGuiTableFlags flags =
			ImGuiTableFlags_Resizable |
			ImGuiTableFlags_BordersInnerV;

		if (ImGui::BeginTable("##BrowserLayout", 2, flags))
		{
			ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 220.0f);
			ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			if (ImGui::BeginChild("##Folders"))
			{
				if (m_folders.size() >= 2)
				{
					DrawFolderTree(0);
					DrawFolderTree(1);
				}
			}

			ImGui::EndChild();
			ImGui::TableSetColumnIndex(1);

			if (ImGui::BeginChild("##Files"))
			{
				const std::string folderLabel = DisplayPath(fs::path(m_selectedFolderKey));
				ImGui::TextUnformatted(folderLabel.c_str());

				ImVec2 listSize;
				listSize.y = -ImGui::GetTextLineHeightWithSpacing() * 4.0f;

				if (ImGui::BeginChild("##Entries", listSize))
				{
					DrawAssetList();
					DrawCreateMenu(callbacks);
				}

				ImGui::EndChild();
				ImGui::Separator();
				DrawSelection();
			}

			ImGui::EndChild();
			ImGui::EndTable();
		}

		DrawCreateDialog(callbacks);
	}

	ImGui::End();
}